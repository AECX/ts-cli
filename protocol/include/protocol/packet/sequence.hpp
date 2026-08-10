#ifndef TS_PROTOCOL_PACKET_SEQUENCE_HPP
#define TS_PROTOCOL_PACKET_SEQUENCE_HPP

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ts::protocol {

    struct PacketSequence {
        std::uint16_t packetId = 0;
        std::uint32_t generationId = 0;

        void Advance() {
            if ( packetId == std::numeric_limits<std::uint16_t>::max() ) {
                if ( generationId == std::numeric_limits<std::uint32_t>::max() ) {
                    throw std::runtime_error( "Packet generation overflow" );
                }

                packetId = 0;

                ++generationId;

                return;
            }

            ++packetId;
        }
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_SEQUENCE_HPP
