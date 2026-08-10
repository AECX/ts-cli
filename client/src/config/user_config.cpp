#include <charconv>
#include <client/config/user_config.hpp>
#include <client/platform/secure_file.hpp>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::client {

    namespace {

        constexpr std::uint64_t MaxUserConfigSize = 64 * 1024;

        [[nodiscard]] bool ParseBoolean( std::string_view value ) {
            if ( value == "true" ) {
                return true;
            }
            if ( value == "false" ) {
                return false;
            }
            throw std::runtime_error( "Invalid user muted value" );
        }

        [[nodiscard]] float ParseFloat( std::string_view value ) {
            float result = 0.0F;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars( first, last, result );
            if ( parsed.ec != std::errc {} || parsed.ptr != last || !std::isfinite( result ) ) {
                throw std::runtime_error( "Invalid user volume value" );
            }
            return result;
        }

    } // namespace

    UserConfigStore::UserConfigStore( std::filesystem::path directory ): m_Directory( std::move( directory ) ) {
    }

    UserConfig UserConfigStore::Load( std::string_view uniqueId ) const {
        Validate( uniqueId, {} );
        const std::filesystem::path path = PathFor( uniqueId );
        const auto data = platform::ReadSecureFile( path, MaxUserConfigSize );
        if ( !data ) {
            return {};
        }

        UserConfig config;
        bool sawUid = false;
        std::istringstream stream( *data );
        std::string line;
        while ( std::getline( stream, line ) ) {
            if ( !line.empty() && line.back() == '\r' ) {
                line.pop_back();
            }
            if ( line.empty() || line.front() == '#' ) {
                continue;
            }
            const std::size_t separator = line.find( '=' );
            if ( separator == std::string::npos ) {
                throw std::runtime_error( "Invalid user config: " + path.string() );
            }
            const std::string_view name( line.data(), separator );
            const std::string_view value( line.data() + separator + 1, line.size() - separator - 1 );
            if ( name == "identity_uid" ) {
                if ( value != uniqueId ) {
                    throw std::runtime_error( "User config identity does not match its filename" );
                }
                sawUid = true;
            } else if ( name == "volume_db" ) {
                config.volumeDb = ParseFloat( value );
            } else if ( name == "muted" ) {
                config.muted = ParseBoolean( value );
            } else {
                throw std::runtime_error( "Unknown user config key: " + std::string( name ) );
            }
        }

        if ( !sawUid ) {
            throw std::runtime_error( "User config is missing identity_uid" );
        }
        Validate( uniqueId, config );
        return config;
    }

    void UserConfigStore::Save( std::string_view uniqueId, const UserConfig& config ) const {
        Validate( uniqueId, config );
        const std::filesystem::path path = PathFor( uniqueId );

        if ( IsDefault( config ) ) {
            std::error_code error;
            std::filesystem::remove( path, error );
            if ( error ) {
                throw std::runtime_error( "Failed to remove default user config: " + error.message() );
            }
            return;
        }

        std::ostringstream stream;
        stream << "# ts-cli remote user settings" << std::endl
               << "identity_uid=" << uniqueId << std::endl
               << std::fixed << std::setprecision( 2 ) << "volume_db=" << config.volumeDb << std::endl
               << "muted=" << ( config.muted ? "true" : "false" ) << std::endl;
        platform::WriteSecureFile( path, stream.str() );
    }

    bool UserConfigStore::IsDefault( const UserConfig& config ) {
        return config.volumeDb == 0.0F && !config.muted;
    }

    float UserConfigStore::MinVolumeDb() {
        return -60.0F;
    }

    float UserConfigStore::MaxVolumeDb() {
        return 12.0F;
    }

    std::filesystem::path UserConfigStore::PathFor( std::string_view uniqueId ) const {
        return m_Directory / ( EncodeFileName( uniqueId ) + ".conf" );
    }

    std::string UserConfigStore::EncodeFileName( std::string_view uniqueId ) {
        static constexpr char Hex[] = "0123456789ABCDEF";
        std::string result;
        result.reserve( uniqueId.size() );

        for ( const char character : uniqueId ) {
            const auto byte = static_cast<unsigned char>( character );
            const bool safe = ( byte >= 'a' && byte <= 'z' ) || ( byte >= 'A' && byte <= 'Z' ) ||
                              ( byte >= '0' && byte <= '9' ) || byte == '-' || byte == '_' || byte == '.';
            if ( safe ) {
                result.push_back( character );
                continue;
            }
            result.push_back( '%' );
            result.push_back( Hex[( byte >> 4U ) & 0x0FU] );
            result.push_back( Hex[byte & 0x0FU] );
        }

        if ( result.empty() ) {
            throw std::runtime_error( "Remote user identity is empty" );
        }
        return result;
    }

    void UserConfigStore::Validate( std::string_view uniqueId, const UserConfig& config ) {
        if ( uniqueId.empty() ) {
            throw std::runtime_error( "Remote user identity is empty" );
        }
        for ( const char character : uniqueId ) {
            if ( character == '\0' || character == '\n' || character == '\r' ) {
                throw std::runtime_error( "Remote user identity contains an invalid character" );
            }
        }
        if ( !std::isfinite( config.volumeDb ) || config.volumeDb < MinVolumeDb() || config.volumeDb > MaxVolumeDb() ) {
            throw std::runtime_error( "User volume must be between -60 and +12 dB" );
        }
    }

} // namespace ts::client
