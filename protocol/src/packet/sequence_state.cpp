#include <cstddef>
#include <cstdint>
#include <protocol/packet/sequence_state.hpp>
#include <stdexcept>

namespace ts::protocol {

    PacketSequenceState::PacketSequenceState() {
        for ( PacketSequence& sequence : m_Outgoing ) {
            sequence.packetId = 1;
        }

        for ( PacketSequence& sequence : m_Incoming ) {
            sequence.packetId = 1;
        }
    }

    PacketSequence& PacketSequenceState::Outgoing( PacketType type ) {
        return m_Outgoing[Index( type )];
    }

    const PacketSequence& PacketSequenceState::Outgoing( PacketType type ) const {
        return m_Outgoing[Index( type )];
    }

    PacketSequence& PacketSequenceState::Incoming( PacketType type ) {
        return m_Incoming[Index( type )];
    }

    const PacketSequence& PacketSequenceState::Incoming( PacketType type ) const {
        return m_Incoming[Index( type )];
    }

    std::size_t PacketSequenceState::Index( PacketType type ) {
        const auto value = static_cast<std::uint8_t>( type );

        if ( value >= TrackedPacketTypeCount ) {
            throw std::runtime_error( "Packet type does not use normal packet sequence state" );
        }

        return static_cast<std::size_t>( value );
    }

} // namespace ts::protocol
