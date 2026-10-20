#ifndef TS_PROTOCOL_CRYPTO_HASH_CASH_HPP
#define TS_PROTOCOL_CRYPTO_HASH_CASH_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>

namespace ts::protocol {

    class HashCash {
      public:
        /* Called with the next offset to try, every ProgressInterval() offsets. */
        using ProgressCallback = std::function<void( std::uint64_t offset )>;

        [[nodiscard]] static std::uint64_t FindOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel );

        /*
         * The first offset from `start` on that reaches `targetLevel`, or
         * std::nullopt once `stop` is requested. Each level doubles the
         * expected number of offsets to try (about 2^level).
         */
        [[nodiscard]] static std::optional<std::uint64_t> FindOffset( std::span<const std::byte> publicKey,
                                                                      std::uint8_t targetLevel,
                                                                      std::uint64_t start,
                                                                      const std::stop_token& stop,
                                                                      const ProgressCallback& progress = {} );

        [[nodiscard]] static std::uint64_t ProgressInterval();

        [[nodiscard]] static bool
            ValidateOffset( std::span<const std::byte> publicKey, std::uint8_t targetLevel, std::uint64_t offset );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_HASH_CASH_HPP
