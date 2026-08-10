#ifndef TS_PROTOCOL_HANDSHAKE_CLIENT_EK_HPP
#define TS_PROTOCOL_HANDSHAKE_CLIENT_EK_HPP

#include <array>
#include <cstddef>
#include <protocol/identity.hpp>
#include <vector>

namespace ts::protocol {

    class ClientEk {
      public:
        ClientEk( const Identity& identity,
                  const std::array<std::byte, 32>& clientPublicKey,
                  const std::array<std::byte, 54>& beta );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        std::vector<std::byte> m_Data;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_CLIENT_EK_HPP
