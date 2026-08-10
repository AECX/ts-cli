#ifndef TS_PROTOCOL_PACKET_INIT_HEADER_HPP
#define TS_PROTOCOL_PACKET_INIT_HEADER_HPP

#include <cstdint>

namespace ts::protocol {

    struct ClientInitHeader {
        std::uint16_t packetId;
        std::uint16_t clientId;
        std::uint8_t flags;
        std::uint32_t version;
        std::uint8_t command;
    };

    struct ServerInitHeader {
        std::uint16_t packetId;
        std::uint8_t flags;
        std::uint8_t command;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_INIT_HEADER_HPP
