#ifndef TS_PROTOCOL_HANDSHAKE_PUZZLE_HPP
#define TS_PROTOCOL_HANDSHAKE_PUZZLE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/message.hpp>
#include <protocol/packet/packet.hpp>

namespace ts::protocol {

    class Puzzle: public ServerHandshakeMessage<handshake::PuzzleCommand> {
      public:
        [[nodiscard]] static Puzzle Parse( const Packet& packet );

        [[nodiscard]] const std::array<std::byte, 64>& X() const;

        [[nodiscard]] const std::array<std::byte, 64>& N() const;

        [[nodiscard]] std::uint32_t Level() const;

        [[nodiscard]] const std::array<std::byte, 100>& ServerData() const;

      private:
        std::array<std::byte, 64> m_X {};
        std::array<std::byte, 64> m_N {};
        std::uint32_t m_Level = 0;
        std::array<std::byte, 100> m_ServerData {};
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_PUZZLE_HPP
