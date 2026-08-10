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

    /*
     * Resolves which 32-bit generation a received 16-bit packetId belongs to,
     * given the next packetId/generation this stream expects. Shared by every
     * independent reliable/ordered stream (Command, CommandLow, Ack, AckLow,
     * Voice, VoiceWhisper), since each tracks its own PacketSequence but the
     * wraparound-disambiguation math is identical.
     */
    [[nodiscard]] std::uint32_t ResolveGeneration( const PacketSequence& expected, std::uint16_t packetId );

    [[nodiscard]] std::uint64_t SequenceKey( std::uint16_t packetId, std::uint32_t generationId );

    [[nodiscard]] std::uint64_t SequenceKey( const PacketSequence& sequence );

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_SEQUENCE_HPP
