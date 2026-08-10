#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <openssl/evp.h>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/encoding/base64.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ts::protocol {

    class HashCashImpl {
      public:
        [[nodiscard]] static std::uint64_t FindOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel ) {
            const std::string encodedPublicKey = Base64Encode( publicKey );

            for ( std::uint64_t offset = 0;; ++offset ) {
                if ( GetLevel( encodedPublicKey, offset ) >= targetLevel ) {
                    return offset;
                }

                if ( offset == std::numeric_limits<std::uint64_t>::max() ) {
                    break;
                }
            }

            throw std::runtime_error( "Unable to find identity key offset" );
        }

        [[nodiscard]] static bool
            ValidateOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel, std::uint64_t offset ) {
            const std::string encodedPublicKey = Base64Encode( publicKey );

            return GetLevel( encodedPublicKey, offset ) >= targetLevel;
        }

      private:
        [[nodiscard]] static std::uint8_t GetLevel( std::string_view encodedPublicKey, std::uint64_t offset ) {
            std::string input( encodedPublicKey );

            input += std::to_string( offset );

            const auto hash = Sha1( input );

            std::uint8_t level = 0;

            for ( const std::byte value : hash ) {
                std::uint8_t byte = std::to_integer<std::uint8_t>( value );

                if ( byte == 0 ) {
                    level += 8;

                    continue;
                }

                while ( ( byte & 0x01 ) == 0 ) {
                    ++level;

                    byte >>= 1;
                }

                break;
            }

            return level;
        }

        [[nodiscard]] static std::array<std::byte, 20> Sha1( std::string_view data ) {
            std::array<std::byte, 20> result {};

            std::size_t resultSize = result.size();

            if ( EVP_Q_digest( nullptr,
                               "SHA1",
                               nullptr,
                               data.data(),
                               data.size(),
                               reinterpret_cast<unsigned char*>( result.data() ),
                               &resultSize ) != 1 ) {
                throw std::runtime_error( "Failed to calculate hashcash SHA-1" );
            }

            if ( resultSize != result.size() ) {
                throw std::runtime_error( "Unexpected hashcash SHA-1 size" );
            }

            return result;
        }
    };

    std::uint64_t HashCash::FindOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel ) {
        return HashCashImpl::FindOffset( publicKey, targetLevel );
    }

    bool HashCash::ValidateOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel, std::uint64_t offset ) {
        return HashCashImpl::ValidateOffset( publicKey, targetLevel, offset );
    }

} // namespace ts::protocol
