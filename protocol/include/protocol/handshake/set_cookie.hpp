#ifndef TS_PROTOCOL_HANDSHAKE_SET_COOKIE_HPP
#define TS_PROTOCOL_HANDSHAKE_SET_COOKIE_HPP

#include <array>
#include <cstddef>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/message.hpp>
#include <protocol/packet/packet.hpp>

namespace ts::protocol {

    class SetCookie: public ServerHandshakeMessage<handshake::SetCookieCommand> {
      public:
        [[nodiscard]] static SetCookie Parse( const Packet& packet );

        [[nodiscard]] const std::array<std::byte, 16>& ServerData() const;

        [[nodiscard]] const std::array<std::byte, 4>& RandomData() const;

      private:
        std::array<std::byte, 16> m_ServerData {};
        std::array<std::byte, 4> m_RandomData {};
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_SET_COOKIE_HPP
