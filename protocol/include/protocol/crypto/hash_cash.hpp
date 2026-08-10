#ifndef TS_PROTOCOL_CRYPTO_HASH_CASH_HPP
#define TS_PROTOCOL_CRYPTO_HASH_CASH_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace ts::protocol {

    class HashCash {
      public:
        [[nodiscard]] static std::uint64_t FindOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel );

        [[nodiscard]] static bool
            ValidateOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel, std::uint64_t offset );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_HASH_CASH_HPP
