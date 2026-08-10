#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <protocol/crypto/p256.hpp>
#include <span>
#include <stdexcept>

namespace ts::protocol {

    class P256Impl {
      public:
        struct KeyDeleter {
            void operator()( EVP_PKEY* key ) const {
                EVP_PKEY_free( key );
            }
        };

        struct KeyContextDeleter {
            void operator()( EVP_PKEY_CTX* context ) const {
                EVP_PKEY_CTX_free( context );
            }
        };

        struct DigestContextDeleter {
            void operator()( EVP_MD_CTX* context ) const {
                EVP_MD_CTX_free( context );
            }
        };

        using Key = std::unique_ptr<EVP_PKEY, KeyDeleter>;

        using KeyContext = std::unique_ptr<EVP_PKEY_CTX, KeyContextDeleter>;

        using DigestContext = std::unique_ptr<EVP_MD_CTX, DigestContextDeleter>;

        class DerReader {
          public:
            explicit DerReader( std::span<const std::byte> data ): m_Data( data ) {
            }

            [[nodiscard]] std::span<const std::byte> Read( std::uint8_t expectedTag ) {
                Require( 1 );

                const std::uint8_t tag = std::to_integer<std::uint8_t>( m_Data[m_Offset++] );

                if ( tag != expectedTag ) {
                    throw std::runtime_error( "Unexpected DER tag" );
                }

                const std::size_t length = ReadLength();

                Require( length );

                const auto result = m_Data.subspan( m_Offset, length );

                m_Offset += length;

                return result;
            }

            [[nodiscard]] bool Empty() const {
                return m_Offset == m_Data.size();
            }

          private:
            [[nodiscard]] std::size_t ReadLength() {
                Require( 1 );

                const std::uint8_t first = std::to_integer<std::uint8_t>( m_Data[m_Offset++] );

                if ( ( first & 0x80 ) == 0 ) {
                    return first;
                }

                const std::size_t byteCount = first & 0x7f;

                if ( byteCount == 0 || byteCount > sizeof( std::size_t ) ) {
                    throw std::runtime_error( "Unsupported DER length" );
                }

                Require( byteCount );

                std::size_t result = 0;

                for ( std::size_t i = 0; i < byteCount; ++i ) {
                    result = ( result << 8 ) | std::to_integer<std::uint8_t>( m_Data[m_Offset++] );
                }

                return result;
            }

            void Require( std::size_t count ) const {
                if ( m_Offset > m_Data.size() || count > m_Data.size() - m_Offset ) {
                    throw std::runtime_error( "Unexpected end of DER data" );
                }
            }

            std::span<const std::byte> m_Data;
            std::size_t m_Offset = 0;
        };

        [[nodiscard]] static Key ParsePublicKey( std::span<const std::byte> encoded ) {
            DerReader outer( encoded );

            const auto sequence = outer.Read( 0x30 );

            if ( !outer.Empty() ) {
                throw std::runtime_error( "Unexpected trailing public key data" );
            }

            DerReader reader( sequence );

            const auto bitString = reader.Read( 0x03 );

            // TomCrypt's public-key marker is one zero bit:
            //
            // DER:
            //   03 02 07 00
            //
            // 07 = seven unused bits
            // 00 = the one meaningful bit is zero
            if ( bitString.size() != 2 || bitString[0] != std::byte { 0x07 } || bitString[1] != std::byte { 0x00 } ) {
                throw std::runtime_error( "Invalid TeamSpeak P-256 key marker" );
            }

            const auto keySize = reader.Read( 0x02 );

            if ( keySize.size() != 1 || keySize[0] != std::byte { 0x20 } ) {
                throw std::runtime_error( "Invalid TeamSpeak P-256 key size" );
            }

            const auto xInteger = reader.Read( 0x02 );

            const auto yInteger = reader.Read( 0x02 );

            if ( !reader.Empty() ) {
                throw std::runtime_error( "Unexpected TeamSpeak public key fields" );
            }

            const auto x = DecodeCoordinate( xInteger );

            const auto y = DecodeCoordinate( yInteger );

            std::array<std::byte, 65> point {};

            point[0] = std::byte { 0x04 };

            std::copy( x.begin(), x.end(), point.begin() + 1 );

            std::copy( y.begin(), y.end(), point.begin() + 33 );

            KeyContext context( EVP_PKEY_CTX_new_from_name( nullptr, "EC", nullptr ) );

            if ( !context ) {
                throw std::runtime_error( "Failed to create P-256 context" );
            }

            if ( EVP_PKEY_fromdata_init( context.get() ) != 1 ) {
                throw std::runtime_error( "Failed to initialize P-256 import" );
            }

            char groupName[] = "prime256v1";

            OSSL_PARAM parameters[] = {
                OSSL_PARAM_construct_utf8_string( OSSL_PKEY_PARAM_GROUP_NAME, groupName, 0 ),
                OSSL_PARAM_construct_octet_string( OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size() ),
                OSSL_PARAM_construct_end() };

            EVP_PKEY* rawKey = nullptr;

            if ( EVP_PKEY_fromdata( context.get(), &rawKey, EVP_PKEY_PUBLIC_KEY, parameters ) != 1 ) {
                throw std::runtime_error( "Failed to import P-256 public key" );
            }

            return Key( rawKey );
        }

        [[nodiscard]] static std::array<std::byte, 32> DecodeCoordinate( std::span<const std::byte> integer ) {
            if ( integer.empty() ) {
                throw std::runtime_error( "Empty P-256 coordinate" );
            }

            // DER INTEGER is signed. Coordinates must be
            // positive, so a high-bit value must have been
            // prefixed with 00.
            if ( ( std::to_integer<std::uint8_t>( integer.front() ) & 0x80 ) != 0 ) {
                throw std::runtime_error( "Negative P-256 coordinate" );
            }

            while ( integer.size() > 1 && integer.front() == std::byte { 0x00 } ) {
                integer = integer.subspan( 1 );
            }

            if ( integer.size() > 32 ) {
                throw std::runtime_error( "P-256 coordinate is too large" );
            }

            std::array<std::byte, 32> result {};

            std::copy( integer.begin(), integer.end(), result.end() - integer.size() );

            return result;
        }

        [[nodiscard]] static bool Verify( std::span<const std::byte> publicKey,
                                          std::span<const std::byte> data,
                                          std::span<const std::byte> signature ) {
            const Key key = ParsePublicKey( publicKey );

            DigestContext context( EVP_MD_CTX_new() );

            if ( !context ) {
                throw std::runtime_error( "Failed to create signature context" );
            }

            if ( EVP_DigestVerifyInit( context.get(), nullptr, EVP_sha256(), nullptr, key.get() ) != 1 ) {
                throw std::runtime_error( "Failed to initialize P-256 verification" );
            }

            const int result = EVP_DigestVerify( context.get(),
                                                 reinterpret_cast<const unsigned char*>( signature.data() ),
                                                 signature.size(),
                                                 reinterpret_cast<const unsigned char*>( data.data() ),
                                                 data.size() );

            if ( result < 0 ) {
                throw std::runtime_error( "P-256 signature verification failed" );
            }

            return result == 1;
        }
    };

    bool P256::Verify( std::span<const std::byte> publicKey,
                       std::span<const std::byte> data,
                       std::span<const std::byte> signature ) {
        return P256Impl::Verify( publicKey, data, signature );
    }

} // namespace ts::protocol
