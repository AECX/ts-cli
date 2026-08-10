#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/sequence.hpp>
#include <protocol/packet/sequence_state.hpp>
#include <stdexcept>

namespace ts::test {

    void RunPacketSequenceTests() {
        {
            const protocol::PacketSequence sequence;

            ExpectEqual( sequence.packetId, std::uint16_t { 0 }, "A standalone packet sequence should be neutral" );

            ExpectEqual( sequence.generationId, std::uint32_t { 0 }, "A standalone packet sequence has an invalid generation" );
        }

        {
            protocol::PacketSequence sequence { .packetId = 41, .generationId = 7 };

            sequence.Advance();

            ExpectEqual( sequence.packetId, std::uint16_t { 42 }, "Packet ID did not advance" );

            ExpectEqual( sequence.generationId, std::uint32_t { 7 }, "Generation changed without packet-ID wrap" );
        }

        {
            protocol::PacketSequence sequence { .packetId = std::numeric_limits<std::uint16_t>::max(), .generationId = 9 };

            sequence.Advance();

            ExpectEqual( sequence.packetId, std::uint16_t { 0 }, "Packet ID did not wrap to zero" );

            ExpectEqual( sequence.generationId, std::uint32_t { 10 }, "Generation did not advance on packet-ID wrap" );
        }

        {
            protocol::PacketSequence sequence { .packetId = std::numeric_limits<std::uint16_t>::max(),

                                                .generationId = std::numeric_limits<std::uint32_t>::max() };

            ExpectThrows<std::runtime_error>(
                [&sequence]() {
                    sequence.Advance();
                },
                "Generation overflow did not fail" );
        }

        {
            protocol::PacketSequenceState state;

            constexpr std::array<protocol::PacketType, 8> PacketTypes = { protocol::PacketType::Voice,
                                                                          protocol::PacketType::VoiceWhisper,
                                                                          protocol::PacketType::Command,
                                                                          protocol::PacketType::CommandLow,
                                                                          protocol::PacketType::Ping,
                                                                          protocol::PacketType::Pong,
                                                                          protocol::PacketType::Ack,
                                                                          protocol::PacketType::AckLow };

            for ( const protocol::PacketType type : PacketTypes ) {
                ExpectEqual( state.Outgoing( type ).packetId,
                             std::uint16_t { 1 },
                             "Outgoing TeamSpeak packet sequence did not start at 1" );

                ExpectEqual( state.Outgoing( type ).generationId,
                             std::uint32_t { 0 },
                             "Outgoing TeamSpeak generation did not start at 0" );

                ExpectEqual( state.Incoming( type ).packetId,
                             std::uint16_t { 1 },
                             "Incoming TeamSpeak packet sequence did not start at 1" );

                ExpectEqual( state.Incoming( type ).generationId,
                             std::uint32_t { 0 },
                             "Incoming TeamSpeak generation did not start at 0" );
            }
        }

        {
            protocol::PacketSequenceState state;

            state.Outgoing( protocol::PacketType::Command ).Advance();

            ExpectEqual( state.Outgoing( protocol::PacketType::Command ).packetId,
                         std::uint16_t { 2 },
                         "Command packet sequence did not advance" );

            ExpectEqual( state.Outgoing( protocol::PacketType::Ack ).packetId,
                         std::uint16_t { 1 },
                         "Command sequence incorrectly changed Ack sequence" );

            ExpectEqual( state.Incoming( protocol::PacketType::Command ).packetId,
                         std::uint16_t { 1 },
                         "Outgoing sequence incorrectly changed incoming sequence" );
        }

        {
            protocol::PacketSequenceState state;

            ExpectThrows<std::runtime_error>(
                [&state]() {
                    (void)state.Outgoing( protocol::PacketType::Init1 );
                },
                "Init1 incorrectly received normal packet sequence state" );

            ExpectThrows<std::runtime_error>(
                [&state]() {
                    (void)state.Incoming( protocol::PacketType::Init1 );
                },
                "Incoming Init1 incorrectly received normal packet sequence state" );
        }
    }

} // namespace ts::test
