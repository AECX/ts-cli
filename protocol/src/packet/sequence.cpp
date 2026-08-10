#include <limits>
#include <protocol/packet/sequence.hpp>
#include <stdexcept>

namespace ts::protocol {

    std::uint32_t ResolveGeneration( const PacketSequence& expected, std::uint16_t packetId ) {
        constexpr std::uint32_t SequenceSize = 65536;
        constexpr std::uint32_t HalfSequenceSize = SequenceSize / 2;

        const std::uint32_t expectedId = expected.packetId;
        const std::uint32_t receivedId = packetId;

        const std::uint32_t forwardDistance = ( receivedId + SequenceSize - expectedId ) % SequenceSize;

        if ( forwardDistance == 0 ) {
            return expected.generationId;
        }

        if ( forwardDistance == HalfSequenceSize ) {
            throw std::runtime_error( "Ambiguous server packet generation" );
        }

        if ( forwardDistance < HalfSequenceSize ) {
            if ( packetId < expected.packetId ) {
                if ( expected.generationId == std::numeric_limits<std::uint32_t>::max() ) {
                    throw std::runtime_error( "Packet generation overflow" );
                }

                return expected.generationId + 1;
            }

            return expected.generationId;
        }

        if ( packetId > expected.packetId ) {
            if ( expected.generationId == 0 ) {
                throw std::runtime_error( "Server packet belongs to an invalid previous generation" );
            }

            return expected.generationId - 1;
        }

        return expected.generationId;
    }

    std::uint64_t SequenceKey( std::uint16_t packetId, std::uint32_t generationId ) {
        return ( static_cast<std::uint64_t>( generationId ) << 16 ) | static_cast<std::uint64_t>( packetId );
    }

    std::uint64_t SequenceKey( const PacketSequence& sequence ) {
        return SequenceKey( sequence.packetId, sequence.generationId );
    }

} // namespace ts::protocol
