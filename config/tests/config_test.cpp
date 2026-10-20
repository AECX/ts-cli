#include "test_support.hpp"

#include <audio/audio_types.hpp>
#include <config/config.hpp>
#include <config/identity_store.hpp>
#include <config/paths.hpp>
#include <filesystem>
#include <fstream>
#include <protocol/identity.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace ts::config::test {

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
        /* Raw POSIX mode bits aren't meaningful on Windows; see config/platform/secure_file.hpp. */
        struct stat configInformation {};
        Expect( ::stat( paths.ConfigFile().c_str(), &configInformation ) == 0, "Could not stat config file" );
        Expect( ( configInformation.st_mode & 0777 ) == 0600, "Config file permissions are not 0600" );

        struct stat directoryInformation {};
        Expect( ::stat( paths.ConfigDirectory().c_str(), &directoryInformation ) == 0, "Could not stat config directory" );
        Expect( ( directoryInformation.st_mode & 0777 ) == 0700, "Config directory permissions are not 0700" );
#endif

        const LocalIdentity localIdentity = IdentityStore::Create( protocol::Identity {}, 2 );
        IdentityStore::Save( paths.IdentityFile(), localIdentity );
        const LocalIdentity loadedIdentity = IdentityStore::Load( paths.IdentityFile() );
        Expect( loadedIdentity.identity.PublicKey() == localIdentity.identity.PublicKey(), "Saved local identity changed" );
        ExpectEqual( loadedIdentity.securityLevel, localIdentity.securityLevel, "Saved identity security level changed" );
        ExpectEqual( loadedIdentity.keyOffset, localIdentity.keyOffset, "Saved identity key offset changed" );

#ifndef _WIN32
        struct stat identityInformation {};
        Expect( ::stat( paths.IdentityFile().c_str(), &identityInformation ) == 0, "Could not stat identity file" );
        Expect( ( identityInformation.st_mode & 0777 ) == 0600, "Identity file permissions are not 0600" );
#endif

        {
            std::ofstream stream( paths.ConfigFile(), std::ios::trunc );
            if ( !stream ) {
                throw std::runtime_error( "Failed to create minimal test config" );
            }
            stream << "nickname=minimal" << std::endl;
        }

        Config minimal = Config::Load( paths.ConfigFile() );
        ExpectEqual( minimal.Profile().nickname, std::string( "minimal" ), "Minimal config nickname changed" );
        ExpectEqual( minimal.Profile().version.version,
                     Config::DefaultProfile().version.version,
                     "Minimal config did not fall back to the default client version" );
        ExpectEqual( minimal.AudioSettings().captureFilter,
                     std::string( "none" ),
                     "Minimal config did not fall back to default audio settings" );

        for ( const std::string_view removedKey :
              { "version_profile", "identity_security_level", "identity_private_key", "identity_key_offset" } ) {
            {
                std::ofstream stream( paths.ConfigFile(), std::ios::trunc );
                if ( !stream ) {
                    throw std::runtime_error( "Failed to create removed-key test config" );
                }
                stream << "nickname=removed" << std::endl << removedKey << "=1" << std::endl;
            }

            bool rejected = false;
            try {
                (void)Config::Load( paths.ConfigFile() );
            } catch ( const std::runtime_error& ) {
                rejected = true;
            }
            Expect( rejected, "Config accepted removed key " + std::string( removedKey ) );
        }

        minimal.SetNickname( "newnick" );
        minimal.Save();
        const Config reloadedAfterBoundSave = Config::Load( paths.ConfigFile() );
        ExpectEqual( reloadedAfterBoundSave.Profile().nickname, std::string( "newnick" ), "Bound-path Save() did not persist" );

        minimal.Update( []( Config& config ) {
            config.SetNickname( "updated" );
        } );
        ExpectEqual( Config::Load( paths.ConfigFile() ).Profile().nickname,
                     std::string( "updated" ),
                     "Update() did not persist the mutation" );

        bool rejectedEmptyNickname = false;
        try {
            minimal.SetNickname( "" );
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

} // namespace ts::config::test
