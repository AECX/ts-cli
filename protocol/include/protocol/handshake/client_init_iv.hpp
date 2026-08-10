#ifndef TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_IV_HPP
#define TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_IV_HPP

#include <array>
#include <cstddef>
#include <protocol/identity.hpp>
#include <string>
#include <vector>

namespace ts::protocol {

    class ClientInitIv {
      public:
        ClientInitIv( const Identity& identity, std::string serverIp );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

        [[nodiscard]] const std::array<std::byte, 10>& Alpha() const;

      private:
        [[nodiscard]] static std::array<std::byte, 10> GenerateAlpha();

        const Identity& m_Identity;
        std::string m_ServerIp;
        std::array<std::byte, 10> m_Alpha;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_IV_HPP
