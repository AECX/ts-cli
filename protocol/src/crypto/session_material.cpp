#include <algorithm>
#include <array>
#include <cstddef>
#include <openssl/evp.h>
#include <protocol/crypto/session_material.hpp>
#include <sodium.h>
#include <span>
#include <stdexcept>

namespace ts::protocol {

    class SessionMaterialDeriverImpl {
      public:
        using Point = std::array<std::byte, 32>;

        using Scalar = std::array<std::byte, 32>;

        [[nodiscard]] static SessionMaterial
            Derive( const License& license, const std::array<std::byte, 10>& alpha, const std::array<std::byte, 54>& beta ) {
            EnsureSodium();

            const Point serverPublicKey = DeriveServerPublicKey( license );

            const Scalar clientPrivateKey = GeneratePrivateKey();

            const Point clientPublicKey = MultiplyBase( clientPrivateKey );

            const Point sharedData = Multiply( clientPrivateKey, serverPublicKey );

            SessionMaterial result;

            result.sharedIv = Sha512( sharedData );

            for ( std::size_t i = 0; i < alpha.size(); ++i ) {
                result.sharedIv[i] ^= alpha[i];
            }

            for ( std::size_t i = 0; i < beta.size(); ++i ) {
                result.sharedIv[alpha.size() + i] ^= beta[i];
            }

            const auto macHash = Sha1( result.sharedIv );

            std::copy_n( macHash.begin(), result.sharedMac.size(), result.sharedMac.begin() );

            result.clientPublicKey = clientPublicKey;

            return result;
        }

      private:
        static constexpr Point Root = { std::byte { 0xcd }, std::byte { 0x0d }, std::byte { 0xe2 }, std::byte { 0xae },
                                        std::byte { 0xd4 }, std::byte { 0x63 }, std::byte { 0x45 }, std::byte { 0x50 },
                                        std::byte { 0x9a }, std::byte { 0x7e }, std::byte { 0x3c }, std::byte { 0xfd },
                                        std::byte { 0x8f }, std::byte { 0x68 }, std::byte { 0xb3 }, std::byte { 0xdc },
                                        std::byte { 0x75 }, std::byte { 0x55 }, std::byte { 0xb2 }, std::byte { 0x9d },
                                        std::byte { 0xcc }, std::byte { 0xec }, std::byte { 0x73 }, std::byte { 0xcd },
                                        std::byte { 0x18 }, std::byte { 0x75 }, std::byte { 0x0f }, std::byte { 0x99 },
                                        std::byte { 0x38 }, std::byte { 0x12 }, std::byte { 0x40 }, std::byte { 0x8a } };

        static void EnsureSodium() {
            if ( sodium_init() < 0 ) {
                throw std::runtime_error( "Failed to initialize libsodium" );
            }
        }

        [[nodiscard]] static Point DeriveServerPublicKey( const License& license ) {
            Point parent = Root;

            for ( const LicenseBlock& block : license.Blocks() ) {
                const Scalar scalar = DeriveBlockScalar( block );

                const Point multiplied = Multiply( scalar, block.PublicKey() );

                Point next {};

                if ( crypto_core_ed25519_add( reinterpret_cast<unsigned char*>( next.data() ),
                                              reinterpret_cast<const unsigned char*>( multiplied.data() ),
                                              reinterpret_cast<const unsigned char*>( parent.data() ) ) != 0 ) {
                    throw std::runtime_error( "Failed to add license curve points" );
                }

                parent = next;
            }

            return parent;
        }

        [[nodiscard]] static Scalar DeriveBlockScalar( const LicenseBlock& block ) {
            auto hash = Sha512( block.HashData() );

            // TeamSpeak's license derivation explicitly
            // clamps the first 32 hash bytes.
            hash[0] &= std::byte { 0xf8 };

            hash[31] &= std::byte { 0x3f };

            hash[31] |= std::byte { 0x40 };

            // ReSpeak then imports those first 32 bytes using
            // Scalar::from_bytes_mod_order(). Reproduce that
            // exactly with libsodium by zero-extending them to
            // a 64-byte non-reduced scalar and reducing mod L.
            std::array<unsigned char, crypto_core_ed25519_NONREDUCEDSCALARBYTES> nonReduced {};

            for ( std::size_t i = 0; i < 32; ++i ) {
                nonReduced[i] = std::to_integer<unsigned char>( hash[i] );
            }

            Scalar result {};

            crypto_core_ed25519_scalar_reduce( reinterpret_cast<unsigned char*>( result.data() ), nonReduced.data() );

            return result;
        }

        [[nodiscard]] static Scalar GeneratePrivateKey() {
            Scalar result {};

            crypto_core_ed25519_scalar_random( reinterpret_cast<unsigned char*>( result.data() ) );

            return result;
        }

        [[nodiscard]] static Point MultiplyBase( const Scalar& scalar ) {
            Point result {};

            if ( crypto_scalarmult_ed25519_base_noclamp( reinterpret_cast<unsigned char*>( result.data() ),
                                                         reinterpret_cast<const unsigned char*>( scalar.data() ) ) != 0 ) {
                throw std::runtime_error( "Failed to derive client public key" );
            }

            return result;
        }

        [[nodiscard]] static Point Multiply( const Scalar& scalar, const Point& point ) {
            Point result {};

            if ( crypto_scalarmult_ed25519_noclamp( reinterpret_cast<unsigned char*>( result.data() ),
                                                    reinterpret_cast<const unsigned char*>( scalar.data() ),
                                                    reinterpret_cast<const unsigned char*>( point.data() ) ) != 0 ) {
                throw std::runtime_error( "Failed to multiply Curve25519 point" );
            }

            return result;
        }

        [[nodiscard]] static std::array<std::byte, 64> Sha512( std::span<const std::byte> data ) {
            std::array<std::byte, 64> result {};

            if ( crypto_hash_sha512( reinterpret_cast<unsigned char*>( result.data() ),
                                     reinterpret_cast<const unsigned char*>( data.data() ),
                                     static_cast<unsigned long long>( data.size() ) ) != 0 ) {
                throw std::runtime_error( "Failed to calculate SHA-512" );
            }

            return result;
        }

        [[nodiscard]] static std::array<std::byte, 20> Sha1( std::span<const std::byte> data ) {
            std::array<std::byte, 20> result {};

            std::size_t resultSize = result.size();

            if ( EVP_Q_digest( nullptr,
                               "SHA1",
                               nullptr,
                               data.data(),
                               data.size(),
                               reinterpret_cast<unsigned char*>( result.data() ),
                               &resultSize ) != 1 ) {
                throw std::runtime_error( "Failed to calculate SHA-1" );
            }

            if ( resultSize != result.size() ) {
                throw std::runtime_error( "Unexpected SHA-1 size" );
            }

            return result;
        }
    };

    SessionMaterial SessionMaterialDeriver::Derive( const License& license,
                                                    const std::array<std::byte, 10>& alpha,
                                                    const std::array<std::byte, 54>& beta ) {
        return SessionMaterialDeriverImpl::Derive( license, alpha, beta );
    }

} // namespace ts::protocol
