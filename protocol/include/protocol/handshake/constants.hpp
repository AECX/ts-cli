#ifndef TS_PROTOCOL_HANDSHAKE_CONSTANTS_HPP
#define TS_PROTOCOL_HANDSHAKE_CONSTANTS_HPP

#include <cstdint>

namespace ts::protocol::handshake {

    inline constexpr std::uint16_t PacketId = 101;
    inline constexpr std::uint16_t InitialClientId = 0;

    inline constexpr std::uint8_t Flags = 0x88;

    inline constexpr std::uint8_t GetCookieCommand = 0x00;
    inline constexpr std::uint8_t SetCookieCommand = 0x01;
    inline constexpr std::uint8_t GetPuzzleCommand = 0x02;
    inline constexpr std::uint8_t PuzzleCommand = 0x03;
    inline constexpr std::uint8_t PuzzleAnswerCommand = 0x04;
    inline constexpr std::uint8_t ResetCommand = 0x7f;

} // namespace ts::protocol::handshake

#endif // TS_PROTOCOL_HANDSHAKE_CONSTANTS_HPP
