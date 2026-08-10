#ifndef TS_PROTOCOL_PACKET_SERVER_HEADER_HPP
#define TS_PROTOCOL_PACKET_SERVER_HEADER_HPP

#include "packet_flags.hpp"
#include "packet_type.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ts::protocol {

    struct ServerPacketHeader {
        std::array<std::byte, 8> mac {};
        std::uint16_t packetId = 0;
        PacketType type = PacketType::Command;
        PacketFlags flags = PacketFlags::None;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_SERVER_HEADER_HPP
