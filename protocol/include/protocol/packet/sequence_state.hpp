#ifndef TS_PROTOCOL_PACKET_SEQUENCE_STATE_HPP
#define TS_PROTOCOL_PACKET_SEQUENCE_STATE_HPP

#include "packet_type.hpp"
#include "sequence.hpp"

#include <array>
#include <cstddef>

namespace ts::protocol {

    class PacketSequenceState {
      public:
        PacketSequenceState();

        [[nodiscard]] PacketSequence& Outgoing( PacketType type );

        [[nodiscard]] const PacketSequence& Outgoing( PacketType type ) const;

        [[nodiscard]] PacketSequence& Incoming( PacketType type );

        [[nodiscard]] const PacketSequence& Incoming( PacketType type ) const;

      private:
        static constexpr std::size_t TrackedPacketTypeCount = 8;

        [[nodiscard]] static std::size_t Index( PacketType type );

        std::array<PacketSequence, TrackedPacketTypeCount> m_Outgoing {};

        std::array<PacketSequence, TrackedPacketTypeCount> m_Incoming {};
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_SEQUENCE_STATE_HPP
