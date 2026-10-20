#include <charconv>
#include <config/identity_store.hpp>
#include <config/platform/secure_file.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/encoding/base64.hpp>
#include <span>
#include <sstream>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace ts::config {

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

    std::uint8_t IdentityStore::MaxSecurityLevel() {
        return 32;
    }

    LocalIdentity IdentityStore::Create( protocol::Identity identity, std::uint8_t securityLevel ) {
        if ( securityLevel > MaxSecurityLevel() ) {
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
                if ( parsed > MaxSecurityLevel() ) {
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

        protocol::Identity identity = DecodeIdentity( privateKey );
        if ( !protocol::HashCash::ValidateOffset( identity.PublicKey(), *securityLevel, *keyOffset ) ) {
            throw std::runtime_error( "Identity key offset is invalid" );
        }

        return LocalIdentity { .identity = std::move( identity ), .securityLevel = *securityLevel, .keyOffset = *keyOffset };
    }

    void IdentityStore::Save( const std::filesystem::path& path, const LocalIdentity& identity ) {
        if ( identity.securityLevel > MaxSecurityLevel() ) {
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

    protocol::Identity IdentityStore::DecodeIdentity( const std::string& encodedPrivateKey ) {
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

    bool IdentityStore::IncreaseSecurityLevel( LocalIdentity& identity,
                                               std::uint8_t targetLevel,
                                               const std::stop_token& stop,
                                               const protocol::HashCash::ProgressCallback& progress ) {
        if ( targetLevel <= identity.securityLevel ) {
            throw std::runtime_error( "The new security level must be higher than the current one (" +
                                      std::to_string( identity.securityLevel ) + ")" );
        }

        if ( targetLevel > MaxSecurityLevel() ) {
            throw std::runtime_error( "Identity security level is too large" );
        }

        const std::optional<std::uint64_t> keyOffset =
            protocol::HashCash::FindOffset( identity.identity.PublicKey(), targetLevel, identity.keyOffset, stop, progress );

        if ( !keyOffset ) {
            return false;
        }

        identity.securityLevel = targetLevel;
        identity.keyOffset = *keyOffset;
        return true;
    }

    protocol::Identity IdentityStore::ImportPem( const std::filesystem::path& path ) {
        std::ifstream stream( path, std::ios::binary );

        if ( !stream ) {
            throw std::runtime_error( "Failed to open PEM file: " + path.string() );
        }

        std::ostringstream data;

        data << stream.rdbuf();

        if ( stream.bad() ) {
            throw std::runtime_error( "Failed to read PEM file: " + path.string() );
        }

        return protocol::Identity::FromPrivateKeyPem( data.str() );
    }

    void IdentityStore::ExportPem( const std::filesystem::path& path, const protocol::Identity& identity ) {
        platform::WriteSecureFile( path, identity.PrivateKeyPem() );
    }

} // namespace ts::config
