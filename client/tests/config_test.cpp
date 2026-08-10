#include "test_support.hpp"

#include <audio/audio_types.hpp>
#include <client/config/config.hpp>
#include <client/config/identity_store.hpp>
#include <client/config/paths.hpp>
#include <filesystem>
#include <fstream>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/encoding/base64.hpp>
#include <protocol/identity.hpp>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace ts::client::test {

    namespace {

        class ConfigTestDirectory {
          public:
            ConfigTestDirectory():
                m_Path( std::filesystem::temp_directory_path() / ( "ts-cli-config-test-" + UniqueTestDirectorySuffix() ) ) {
                std::filesystem::remove_all( m_Path );
                std::filesystem::create_directories( m_Path );
            }

            ~ConfigTestDirectory() {
                std::error_code error;
                std::filesystem::remove_all( m_Path, error );
            }

            [[nodiscard]] const std::filesystem::path& Path() const {
                return m_Path;
            }

          private:
            std::filesystem::path m_Path;
        };

        [[nodiscard]] std::string ReadFile( const std::filesystem::path& path ) {
            std::ifstream stream( path );
            if ( !stream ) {
                throw std::runtime_error( "Failed to read test file" );
            }
            std::ostringstream result;
            result << stream.rdbuf();
            return result.str();
        }

        [[nodiscard]] std::string EncodeIdentity( const protocol::Identity& identity ) {
            const std::string pem = identity.PrivateKeyPem();
            return protocol::Base64Encode( std::as_bytes( std::span<const char>( pem.data(), pem.size() ) ) );
        }

    } // namespace

    void RunConfigTests() {
        ConfigTestDirectory temporary;
        const Paths paths = Paths::FromConfigHome( temporary.Path() / "config" );
        paths.EnsureDirectories();

        protocol::ClientProfile profile = Config::DefaultProfile();
        profile.nickname = "uhinf";

        Config created = Config::Create( profile );
        audio::AudioSettings audioSettings;
        audioSettings.input = "alsa_input.test";
        audioSettings.output = "alsa_output.test";
        audioSettings.captureFilter = "rnnoise";
        audioSettings.activationThresholdDb = -42.5F;
        created.SetAudioSettings( audioSettings );
        created.Save( paths.ConfigFile() );

        const Config loaded = Config::Load( paths.ConfigFile() );
        const protocol::ClientProfile loadedProfile = loaded.Profile();
        ExpectEqual( loadedProfile.nickname, std::string( "uhinf" ), "Config nickname changed" );
        ExpectEqual( loadedProfile.version.initVersion, profile.version.initVersion, "Config Init1 version changed" );
        ExpectEqual( loadedProfile.version.version, profile.version.version, "Config client version changed" );
        ExpectEqual( loadedProfile.version.platform, profile.version.platform, "Config client platform changed" );
        ExpectEqual( loadedProfile.version.signature, profile.version.signature, "Config client signature changed" );

        const audio::AudioSettings loadedAudio = loaded.AudioSettings();
        ExpectEqual( loadedAudio.input, audioSettings.input, "Config audio input changed" );
        ExpectEqual( loadedAudio.output, audioSettings.output, "Config audio output changed" );
        ExpectEqual( loadedAudio.captureFilter, audioSettings.captureFilter, "Config audio filter changed" );
        Expect( loadedAudio.activationThresholdDb == audioSettings.activationThresholdDb,
                "Config audio activation threshold changed" );

        const std::string configData = ReadFile( paths.ConfigFile() );
        Expect( configData.find( "identity_private_key=" ) == std::string::npos,
                "New config unexpectedly contains local identity material" );
        Expect( configData.find( "identity_key_offset=" ) == std::string::npos,
                "New config unexpectedly contains local identity offset" );

#ifndef _WIN32
        /* Raw POSIX mode bits aren't meaningful on Windows; see client/platform/secure_file.hpp. */
        struct stat configInformation {};
        Expect( ::stat( paths.ConfigFile().c_str(), &configInformation ) == 0, "Could not stat config file" );
        Expect( ( configInformation.st_mode & 0777 ) == 0600, "Config file permissions are not 0600" );

        struct stat directoryInformation {};
        Expect( ::stat( paths.ConfigDirectory().c_str(), &directoryInformation ) == 0, "Could not stat config directory" );
        Expect( ( directoryInformation.st_mode & 0777 ) == 0700, "Config directory permissions are not 0700" );
#endif

        protocol::Identity legacyIdentity;
        const std::vector<std::byte> legacyPublicKey = legacyIdentity.PublicKey();
        const std::uint8_t securityLevel = 2;
        const std::uint64_t keyOffset = protocol::HashCash::FindOffset( legacyPublicKey, securityLevel );
        const std::string encodedIdentity = EncodeIdentity( legacyIdentity );

        {
            std::ofstream stream( paths.ConfigFile(), std::ios::trunc );
            if ( !stream ) {
                throw std::runtime_error( "Failed to create legacy test config" );
            }
            stream << "nickname=legacy" << std::endl
                   << "version_profile=ts3-linux-3.6.2" << std::endl
                   << "identity_security_level=" << static_cast<unsigned int>( securityLevel ) << std::endl
                   << "identity_private_key=" << encodedIdentity << std::endl
                   << "identity_key_offset=" << keyOffset << std::endl;
        }

        Config migrated = Config::Load( paths.ConfigFile() );
        Expect( migrated.HasLegacyIdentity(), "Legacy config identity was not detected" );
        Expect( migrated.NeedsSave(), "Legacy config was not marked for migration" );

        const LocalIdentity migratedIdentity = IdentityStore::LoadOrMigrate( paths.IdentityFile(), migrated.LegacyIdentity() );
        Expect( migratedIdentity.identity.PublicKey() == legacyPublicKey, "Migrated local identity changed" );
        ExpectEqual( migratedIdentity.securityLevel, securityLevel, "Migrated identity security level changed" );
        ExpectEqual( migratedIdentity.keyOffset, keyOffset, "Migrated identity key offset changed" );

        migrated.ClearLegacyIdentity();
        migrated.Save( paths.ConfigFile() );
        const std::string migratedConfig = ReadFile( paths.ConfigFile() );
        Expect( migratedConfig.find( "identity_private_key=" ) == std::string::npos,
                "Legacy private key remained in config after migration" );
        Expect( migratedConfig.find( "identity_security_level=" ) == std::string::npos,
                "Legacy identity security level remained in config after migration" );
        Expect( migratedConfig.find( "identity_key_offset=" ) == std::string::npos,
                "Legacy identity offset remained in config after migration" );
        Expect( std::filesystem::exists( paths.IdentityFile() ), "Identity migration did not create identity file" );

#ifndef _WIN32
        struct stat identityInformation {};
        Expect( ::stat( paths.IdentityFile().c_str(), &identityInformation ) == 0, "Could not stat identity file" );
        Expect( ( identityInformation.st_mode & 0777 ) == 0600, "Identity file permissions are not 0600" );
#endif

        migrated.SetNickname( "newnick" );
        migrated.Save();
        const Config reloadedAfterBoundSave = Config::Load( paths.ConfigFile() );
        ExpectEqual( reloadedAfterBoundSave.Profile().nickname, std::string( "newnick" ), "Bound-path Save() did not persist" );

        bool rejectedEmptyNickname = false;
        try {
            migrated.SetNickname( "" );
        } catch ( const std::runtime_error& ) {
            rejectedEmptyNickname = true;
        }
        Expect( rejectedEmptyNickname, "Empty nickname was accepted" );

        bool rejectedUnboundSave = false;
        try {
            Config unbound = Config::Create( Config::DefaultProfile() );
            unbound.Save();
        } catch ( const std::runtime_error& ) {
            rejectedUnboundSave = true;
        }
        Expect( rejectedUnboundSave, "Save() on a Config with no bound path did not throw" );
    }

} // namespace ts::client::test
