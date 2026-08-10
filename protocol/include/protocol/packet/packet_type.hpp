#ifndef TS_PROTOCOL_PACKET_PACKET_TYPE_HPP
#define TS_PROTOCOL_PACKET_PACKET_TYPE_HPP

#include <cstdint>

namespace ts::protocol {

    enum class PacketType : std::uint8_t {
        Voice = 0x00,
        VoiceWhisper = 0x01,
        Command = 0x02,
        CommandLow = 0x03,
        Ping = 0x04,
        Pong = 0x05,
        Ack = 0x06,
        AckLow = 0x07,
        Init1 = 0x08
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_PACKET_TYPE_HPP
