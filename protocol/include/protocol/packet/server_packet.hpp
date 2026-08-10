#ifndef TS_PROTOCOL_PACKET_SERVER_PACKET_HPP
#define TS_PROTOCOL_PACKET_SERVER_PACKET_HPP

#include "packet.hpp"
#include "server_header.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace ts::protocol {

    class ServerPacket {
      public:
        [[nodiscard]] static ServerPacket Parse( const Packet& packet );

        [[nodiscard]] const ServerPacketHeader& Header() const;

        [[nodiscard]] std::span<const std::byte> Meta() const;
        [[nodiscard]] std::span<const std::byte> Data() const;

      private:
        ServerPacketHeader m_Header;
        std::array<std::byte, 3> m_Meta {};
        std::vector<std::byte> m_Data;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_SERVER_PACKET_HPP
