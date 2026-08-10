#ifndef TS_PROTOCOL_CRYPTO_P256_HPP
#define TS_PROTOCOL_CRYPTO_P256_HPP

#include <cstddef>
#include <span>

namespace ts::protocol {

    class P256 {
      public:
        [[nodiscard]] static bool Verify( std::span<const std::byte> publicKey,
                                          std::span<const std::byte> data,
                                          std::span<const std::byte> signature );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_P256_HPP
