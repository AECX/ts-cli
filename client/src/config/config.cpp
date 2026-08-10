#include <charconv>
#include <client/config/config.hpp>
#include <client/platform/secure_file.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/encoding/base64.hpp>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::client {

    namespace {
        constexpr std::uint64_t MaxConfigSize = 1024 * 1024;
    } // namespace

    Config Config::Load( const std::filesystem::path& path ) {
        const auto data = platform::ReadSecureFile( path, MaxConfigSize );

        if ( !data ) {
            throw std::runtime_error( "Config does not exist: " + path.string() );
        }

        bool migrated = false;
        Config config = Parse( *data, migrated );
        config.m_NeedsSave = migrated;
        config.m_Path = path;
        return config;
    }

    Config Config::Create( protocol::ClientProfile profile ) {
        if ( profile.nickname.empty() ) {
            throw std::runtime_error( "Client nickname cannot be empty" );
        }

        Config config;
        config.m_Nickname = std::move( profile.nickname );
        config.m_ClientInitVersion = profile.version.initVersion;
        config.m_ClientVersion = std::move( profile.version.version );
        config.m_ClientPlatform = std::move( profile.version.platform );
        config.m_ClientVersionSignature = std::move( profile.version.signature );
        return config;
    }

    protocol::ClientProfile Config::DefaultProfile() {
        return protocol::ClientProfile {
            .nickname = "ts-cli",

            .version = protocol::ClientVersionProfile {
                .initVersion = 0x142898dd,

                .version = "3.6.2 [Build: 1695203293]",

                .platform = "Linux",

                .signature = "p4iF1jZ3ZOz9MEkKSZ2bvnFtm9WmUcQy9mAP//erFE4PF1sB6K1CSANrr+3X4B0aZR0u+K2pjnv8kiKsWKQCBQ==" } };
    }

    void Config::Save( const std::filesystem::path& path ) {
        m_Path = path;

        std::ostringstream stream;

        stream << "# ts-cli configuration" << std::endl
               << std::endl
               << "nickname=" << m_Nickname << std::endl
               << std::endl
               << "# TeamSpeak wire client profile." << std::endl
               << "# Keep these four fields as one coherent tuple." << std::endl
               << "client_init_version=0x" << std::hex << m_ClientInitVersion << std::dec << std::endl
               << "client_version=" << m_ClientVersion << std::endl
               << "client_platform=" << m_ClientPlatform << std::endl
               << "client_version_sign=" << m_ClientVersionSignature << std::endl
               << std::endl
               << "# Audio settings. Transmission still starts muted each session." << std::endl
               << "audio_input=" << m_AudioSettings.input << std::endl
               << "audio_output=" << m_AudioSettings.output << std::endl
               << "audio_filter=" << m_AudioSettings.captureFilter << std::endl
               << "audio_activation_threshold_db=" << m_AudioSettings.activationThresholdDb << std::endl;

        platform::WriteSecureFile( path, stream.str() );
    }

    void Config::Save() {
        if ( !m_Path ) {
            throw std::runtime_error( "Config has no bound path to save to" );
        }

        Save( *m_Path );
    }

    protocol::ClientProfile Config::Profile() const {
        if ( m_Nickname.empty() ) {
            throw std::runtime_error( "Client nickname cannot be empty" );
        }

        return protocol::ClientProfile { .nickname = m_Nickname,

                                         .version = protocol::ClientVersionProfile { .initVersion = m_ClientInitVersion,

                                                                                     .version = m_ClientVersion,

                                                                                     .platform = m_ClientPlatform,

                                                                                     .signature = m_ClientVersionSignature } };
    }

    audio::AudioSettings Config::AudioSettings() const {
        return m_AudioSettings;
    }

    void Config::SetAudioSettings( const audio::AudioSettings& settings ) {
        if ( !std::isfinite( settings.activationThresholdDb ) || settings.activationThresholdDb < -100.0F ||
             settings.activationThresholdDb > 0.0F ) {
            throw std::runtime_error( "audio_activation_threshold_db must be between -100 and 0" );
        }

        if ( settings.input.empty() || settings.output.empty() || settings.captureFilter.empty() ) {
            throw std::runtime_error( "Audio config values cannot be empty" );
        }

        m_AudioSettings = settings;
    }

    void Config::SetNickname( std::string nickname ) {
        if ( nickname.empty() ) {
            throw std::runtime_error( "Client nickname cannot be empty" );
        }

        m_Nickname = std::move( nickname );
    }

    bool Config::NeedsSave() const {
        return m_NeedsSave;
    }

    bool Config::HasLegacyIdentity() const {
        return m_HasLegacyIdentity;
    }

    LegacyIdentityConfig Config::LegacyIdentity() const {
        return m_LegacyIdentity;
    }

    void Config::ClearLegacyIdentity() {
        m_HasLegacyIdentity = false;
        m_LegacyIdentity = LegacyIdentityConfig {};
        m_NeedsSave = true;
    }

    Config Config::Parse( std::string_view data, bool& migrated ) {
        const protocol::ClientProfile defaultProfile = DefaultProfile();

        Config config;

        config.m_Nickname = defaultProfile.nickname;

        config.m_ClientInitVersion = defaultProfile.version.initVersion;

        config.m_ClientVersion = defaultProfile.version.version;

        config.m_ClientPlatform = defaultProfile.version.platform;

        config.m_ClientVersionSignature = defaultProfile.version.signature;

        config.m_LegacyIdentity.securityLevel = IdentityStore::DefaultSecurityLevel();

        migrated = false;

        std::map<std::string, std::string> values;

        std::size_t lineNumber = 0;

        while ( !data.empty() ) {
            ++lineNumber;

            const std::size_t newline = data.find( '\n' );

            std::string_view line;

            if ( newline == std::string_view::npos ) {
                line = data;

                data = {};
            } else {
                line = data.substr( 0, newline );

                data.remove_prefix( newline + 1 );
            }

            if ( !line.empty() && line.back() == '\r' ) {
                line.remove_suffix( 1 );
            }

            line = Trim( line );

            if ( line.empty() || line.front() == '#' ) {
                continue;
            }

            const std::size_t separator = line.find( '=' );

            if ( separator == std::string_view::npos ) {
                throw std::runtime_error( "Invalid config line " + std::to_string( lineNumber ) );
            }

            const std::string_view name = Trim( line.substr( 0, separator ) );

            const std::string_view value = Trim( line.substr( separator + 1 ) );

            if ( name.empty() ) {
                throw std::runtime_error( "Empty config key on line " + std::to_string( lineNumber ) );
            }

            if ( name != "nickname" && name != "client_init_version" && name != "client_version" && name != "client_platform" &&
                 name != "client_version_sign" && name != "identity_security_level" && name != "identity_private_key" &&
                 name != "identity_key_offset" && name != "version_profile" && name != "audio_input" &&
                 name != "audio_output" && name != "audio_filter" && name != "audio_activation_threshold_db" ) {
                throw std::runtime_error( "Unknown config key: " + std::string( name ) );
            }

            const auto [iterator, inserted] = values.emplace( std::string( name ), std::string( value ) );

            (void)iterator;

            if ( !inserted ) {
                throw std::runtime_error( "Duplicate config key: " + std::string( name ) );
            }
        }

        if ( const auto legacyProfile = values.find( "version_profile" ); legacyProfile != values.end() ) {
            if ( legacyProfile->second == "ts3-linux-3.6.2" ) {
                /*
                 * The default explicit tuple represents the
                 * old built-in profile.
                 */
            } else if ( legacyProfile->second == "custom" ) {
                const bool hasCustomFields = values.contains( "client_init_version" ) || values.contains( "client_version" ) ||
                                             values.contains( "client_platform" ) || values.contains( "client_version_sign" );

                if ( !hasCustomFields ) {
                    throw std::runtime_error( "Legacy custom version profile has no client version fields" );
                }
            } else {
                throw std::runtime_error( "Unsupported legacy version profile: " + legacyProfile->second );
            }

            migrated = true;
        }

        if ( const auto value = values.find( "nickname" ); value != values.end() ) {
            if ( value->second.empty() ) {
                throw std::runtime_error( "Client nickname cannot be empty" );
            }

            config.m_Nickname = value->second;
        }

        if ( const auto value = values.find( "client_init_version" ); value != values.end() ) {
            const std::uint64_t parsed = ParseUnsigned( value->second, "client_init_version" );

            if ( parsed > std::numeric_limits<std::uint32_t>::max() ) {
                throw std::runtime_error( "client_init_version is too large" );
            }

            config.m_ClientInitVersion = static_cast<std::uint32_t>( parsed );
        }

        if ( const auto value = values.find( "client_version" ); value != values.end() ) {
            config.m_ClientVersion = value->second;
        }

        if ( const auto value = values.find( "client_platform" ); value != values.end() ) {
            config.m_ClientPlatform = value->second;
        }

        if ( const auto value = values.find( "client_version_sign" ); value != values.end() ) {
            config.m_ClientVersionSignature = value->second;
        }

        if ( const auto value = values.find( "identity_security_level" ); value != values.end() ) {
            const std::uint64_t parsed = ParseUnsigned( value->second, "identity_security_level" );

            if ( parsed > 32 ) {
                throw std::runtime_error( "identity_security_level is too large" );
            }

            config.m_LegacyIdentity.securityLevel = static_cast<std::uint8_t>( parsed );
            config.m_HasLegacyIdentity = true;
        }

        if ( const auto value = values.find( "identity_private_key" ); value != values.end() ) {
            config.m_LegacyIdentity.encodedPrivateKey = value->second;
            config.m_HasLegacyIdentity = true;
        }

        if ( const auto value = values.find( "identity_key_offset" ); value != values.end() ) {
            if ( value->second.empty() ) {
                config.m_LegacyIdentity.keyOffset = std::nullopt;
            } else {
                config.m_LegacyIdentity.keyOffset = ParseUnsigned( value->second, "identity_key_offset" );
            }
            config.m_HasLegacyIdentity = true;
        }

        (void)config.Profile();

        if ( const auto value = values.find( "audio_input" ); value != values.end() ) {
            if ( value->second.empty() ) {
                throw std::runtime_error( "audio_input cannot be empty" );
            }
            config.m_AudioSettings.input = value->second;
        }

        if ( const auto value = values.find( "audio_output" ); value != values.end() ) {
            if ( value->second.empty() ) {
                throw std::runtime_error( "audio_output cannot be empty" );
            }
            config.m_AudioSettings.output = value->second;
        }

        if ( const auto value = values.find( "audio_filter" ); value != values.end() ) {
            if ( value->second.empty() ) {
                throw std::runtime_error( "audio_filter cannot be empty" );
            }
            config.m_AudioSettings.captureFilter = value->second;
        }

        if ( const auto value = values.find( "audio_activation_threshold_db" ); value != values.end() ) {
            config.m_AudioSettings.activationThresholdDb = ParseFloat( value->second, "audio_activation_threshold_db" );

            if ( config.m_AudioSettings.activationThresholdDb < -100.0F ||
                 config.m_AudioSettings.activationThresholdDb > 0.0F ) {
                throw std::runtime_error( "audio_activation_threshold_db must be between -100 and 0" );
            }
        }

        if ( !values.contains( "audio_input" ) || !values.contains( "audio_output" ) || !values.contains( "audio_filter" ) ||
             !values.contains( "audio_activation_threshold_db" ) ) {
            migrated = true;
        }

        return config;
    }

    std::uint64_t Config::ParseUnsigned( std::string_view value, std::string_view name ) {
        if ( value.empty() ) {
            throw std::runtime_error( "Empty unsigned config value: " + std::string( name ) );
        }

        int base = 10;

        if ( value.size() >= 2 && value[0] == '0' && ( value[1] == 'x' || value[1] == 'X' ) ) {
            value.remove_prefix( 2 );

            base = 16;

            if ( value.empty() ) {
                throw std::runtime_error( "Invalid hexadecimal config value: " + std::string( name ) );
            }
        }

        std::uint64_t result = 0;

        const char* first = value.data();

        const char* last = first + value.size();

        const auto parsed = std::from_chars( first, last, result, base );

        if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
            throw std::runtime_error( "Invalid unsigned config value: " + std::string( name ) );
        }

        return result;
    }

    float Config::ParseFloat( std::string_view value, std::string_view name ) {
        float result = 0.0F;
        const char* begin = value.data();
        const char* end = value.data() + value.size();
        const auto parsed = std::from_chars( begin, end, result );

        if ( parsed.ec != std::errc {} || parsed.ptr != end || !std::isfinite( result ) ) {
            throw std::runtime_error( "Invalid " + std::string( name ) );
        }

        return result;
    }

    std::string_view Config::Trim( std::string_view value ) {
        while ( !value.empty() && ( value.front() == ' ' || value.front() == '\t' ) ) {
            value.remove_prefix( 1 );
        }

        while ( !value.empty() && ( value.back() == ' ' || value.back() == '\t' ) ) {
            value.remove_suffix( 1 );
        }

        return value;
    }

} // namespace ts::client
