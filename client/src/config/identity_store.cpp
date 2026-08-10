#include <charconv>
#include <client/config/identity_store.hpp>
#include <client/platform/secure_file.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/encoding/base64.hpp>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ts::client {

    namespace {

        constexpr std::uint64_t MaxIdentitySize = 1024 * 1024;

        [[nodiscard]] std::string ReadIdentityFile( const std::filesystem::path& path ) {
            const auto data = platform::ReadSecureFile( path, MaxIdentitySize );

            if ( !data ) {
                throw std::runtime_error( "Identity does not exist: " + path.string() );
            }

            return *data;
        }

        [[nodiscard]] std::uint64_t ParseUnsigned( std::string_view value, std::string_view name ) {
            if ( value.empty() ) {
                throw std::runtime_error( "Empty identity value: " + std::string( name ) );
            }

            std::uint64_t result = 0;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars( first, last, result );

            if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
                throw std::runtime_error( "Invalid identity value: " + std::string( name ) );
            }
            return result;
        }

    } // namespace

    std::uint8_t IdentityStore::DefaultSecurityLevel() {
        return 8;
    }

    LocalIdentity IdentityStore::Create( protocol::Identity identity, std::uint8_t securityLevel ) {
        if ( securityLevel > 32 ) {
            throw std::runtime_error( "Identity security level is too large" );
        }

        const std::uint64_t keyOffset = protocol::HashCash::FindOffset( identity.PublicKey(), securityLevel );
        return LocalIdentity { .identity = std::move( identity ), .securityLevel = securityLevel, .keyOffset = keyOffset };
    }

    LocalIdentity IdentityStore::Load( const std::filesystem::path& path ) {
        const std::string data = ReadIdentityFile( path );
        std::optional<std::uint8_t> securityLevel;
        std::optional<std::uint64_t> keyOffset;
        std::string privateKey;
        std::istringstream stream( data );
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
                throw std::runtime_error( "Invalid identity file" );
            }

            const std::string_view name( line.data(), separator );
            const std::string_view value( line.data() + separator + 1, line.size() - separator - 1 );

            if ( name == "security_level" ) {
                const std::uint64_t parsed = ParseUnsigned( value, name );
                if ( parsed > 32 ) {
                    throw std::runtime_error( "Identity security level is too large" );
                }
                securityLevel = static_cast<std::uint8_t>( parsed );
            } else if ( name == "key_offset" ) {
                keyOffset = ParseUnsigned( value, name );
            } else if ( name == "private_key" ) {
                privateKey = std::string( value );
            } else {
                throw std::runtime_error( "Unknown identity key: " + std::string( name ) );
            }
        }

        if ( !securityLevel || !keyOffset || privateKey.empty() ) {
            throw std::runtime_error( "Identity file is incomplete" );
        }

        protocol::Identity identity = DecodeLegacyIdentity( privateKey );
        if ( !protocol::HashCash::ValidateOffset( identity.PublicKey(), *securityLevel, *keyOffset ) ) {
            throw std::runtime_error( "Identity key offset is invalid" );
        }

        return LocalIdentity { .identity = std::move( identity ), .securityLevel = *securityLevel, .keyOffset = *keyOffset };
    }

    LocalIdentity IdentityStore::LoadOrMigrate( const std::filesystem::path& path, const LegacyIdentityConfig& legacy ) {
        if ( std::filesystem::exists( path ) ) {
            return Load( path );
        }

        if ( legacy.encodedPrivateKey.empty() ) {
            throw std::runtime_error( "Cannot migrate an empty TeamSpeak identity" );
        }
        if ( legacy.securityLevel > 32 ) {
            throw std::runtime_error( "Identity security level is too large" );
        }

        protocol::Identity identity = DecodeLegacyIdentity( legacy.encodedPrivateKey );
        std::uint64_t keyOffset = 0;
        if ( legacy.keyOffset &&
             protocol::HashCash::ValidateOffset( identity.PublicKey(), legacy.securityLevel, *legacy.keyOffset ) ) {
            keyOffset = *legacy.keyOffset;
        } else {
            keyOffset = protocol::HashCash::FindOffset( identity.PublicKey(), legacy.securityLevel );
        }

        LocalIdentity result { .identity = std::move( identity ),
                               .securityLevel = legacy.securityLevel,
                               .keyOffset = keyOffset };
        Save( path, result );
        return result;
    }

    void IdentityStore::Save( const std::filesystem::path& path, const LocalIdentity& identity ) {
        if ( identity.securityLevel > 32 ) {
            throw std::runtime_error( "Identity security level is too large" );
        }
        if ( !protocol::HashCash::ValidateOffset( identity.identity.PublicKey(),
                                                  identity.securityLevel,
                                                  identity.keyOffset ) ) {
            throw std::runtime_error( "Identity key offset is invalid" );
        }

        std::ostringstream stream;
        stream << "# ts-cli TeamSpeak identity" << std::endl
               << "security_level=" << static_cast<unsigned int>( identity.securityLevel ) << std::endl
               << "key_offset=" << identity.keyOffset << std::endl
               << "private_key=" << EncodeIdentity( identity.identity ) << std::endl;
        platform::WriteSecureFile( path, stream.str() );
    }

    protocol::Identity IdentityStore::DecodeLegacyIdentity( const std::string& encodedPrivateKey ) {
        if ( encodedPrivateKey.empty() ) {
            throw std::runtime_error( "Identity private key is empty" );
        }
        const std::vector<std::byte> decoded = protocol::Base64Decode( encodedPrivateKey );
        if ( decoded.empty() ) {
            throw std::runtime_error( "Identity private key decoded to empty data" );
        }
        const std::string pem( reinterpret_cast<const char*>( decoded.data() ), decoded.size() );
        return protocol::Identity::FromPrivateKeyPem( pem );
    }

    std::string IdentityStore::EncodeIdentity( const protocol::Identity& identity ) {
        const std::string pem = identity.PrivateKeyPem();
        const std::span<const char> characters( pem.data(), pem.size() );
        return protocol::Base64Encode( std::as_bytes( characters ) );
    }

} // namespace ts::client
