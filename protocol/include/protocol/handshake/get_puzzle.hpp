#ifndef TS_PROTOCOL_HANDSHAKE_GET_PUZZLE_HPP
#define TS_PROTOCOL_HANDSHAKE_GET_PUZZLE_HPP

#include <cstdint>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/message.hpp>
#include <protocol/handshake/set_cookie.hpp>
#include <protocol/packet/packet.hpp>

namespace ts::protocol {

    class GetPuzzle: public ClientHandshakeMessage<handshake::GetPuzzleCommand> {
      public:
        GetPuzzle( const SetCookie& cookie, std::uint32_t clientVersion );

        [[nodiscard]] Packet Serialize() const;

      private:
        SetCookie m_Cookie;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_GET_PUZZLE_HPP
