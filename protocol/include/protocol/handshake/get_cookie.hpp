#ifndef TS_PROTOCOL_HANDSHAKE_GET_COOKIE_HPP
#define TS_PROTOCOL_HANDSHAKE_GET_COOKIE_HPP

#include <cstdint>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/message.hpp>
#include <protocol/packet/packet.hpp>

namespace ts::protocol {

    class GetCookie: public ClientHandshakeMessage<handshake::GetCookieCommand> {
      public:
        explicit GetCookie( std::uint32_t clientVersion );

        [[nodiscard]] Packet Serialize() const;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_GET_COOKIE_HPP
