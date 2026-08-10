#ifndef TS_PROTOCOL_HANDSHAKE_PUZZLE_ANSWER_HPP
#define TS_PROTOCOL_HANDSHAKE_PUZZLE_ANSWER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/handshake/constants.hpp>
#include <protocol/handshake/message.hpp>
#include <protocol/handshake/puzzle.hpp>
#include <protocol/packet/packet.hpp>
#include <vector>

namespace ts::protocol {

    class PuzzleAnswer: public ClientHandshakeMessage<handshake::PuzzleAnswerCommand> {
      public:
        PuzzleAnswer( const Puzzle& puzzle,
                      const std::array<std::byte, 64>& solution,
                      std::vector<std::byte> clientInitIv,
                      std::uint32_t clientVersion );

        [[nodiscard]] Packet Serialize() const;

      private:
        Puzzle m_Puzzle;

        std::array<std::byte, 64> m_Solution;

        std::vector<std::byte> m_ClientInitIv;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_PUZZLE_ANSWER_HPP
