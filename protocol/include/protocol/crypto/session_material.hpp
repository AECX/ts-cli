#ifndef TS_PROTOCOL_CRYPTO_SESSION_MATERIAL_HPP
#define TS_PROTOCOL_CRYPTO_SESSION_MATERIAL_HPP

#include <array>
#include <cstddef>
#include <protocol/handshake/license.hpp>

namespace ts::protocol {

    struct SessionMaterial {
        std::array<std::byte, 64> sharedIv {};
        std::array<std::byte, 8> sharedMac {};
        std::array<std::byte, 32> clientPublicKey {};
    };

    class SessionMaterialDeriver {
      public:
        [[nodiscard]] static SessionMaterial
            Derive( const License& license, const std::array<std::byte, 10>& alpha, const std::array<std::byte, 54>& beta );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_SESSION_MATERIAL_HPP
