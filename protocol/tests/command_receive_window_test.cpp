#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/sequence.hpp>
#include <protocol/reliability/command_receive_window.hpp>
#include <stdexcept>
#include <vector>

namespace ts::test {

    void RunCommandReceiveWindowTests() {
        using protocol::CommandReceiveWindow;
        using protocol::PacketFlags;
        using protocol::PacketSequence;

        // In-order, unfragmented, uncompressed packet is delivered immediately.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            const auto ready = window.Feed( expected, 1, 0, PacketFlags::None, Bytes( "hello" ) );

            ExpectEqual( ready.size(), std::size_t { 1 }, "In-order packet did not produce a ready command" );
            ExpectEqual( ready.front(), Bytes( "hello" ), "In-order packet payload was corrupted" );
            ExpectEqual( expected.packetId, std::uint16_t { 2 }, "Sequence did not advance past the consumed packet" );
        }

        // A duplicate/old packet is dropped without advancing the sequence or throwing.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            (void)window.Feed( expected, 1, 0, PacketFlags::None, Bytes( "first" ) );

            const auto duplicate = window.Feed( expected, 1, 0, PacketFlags::None, Bytes( "first-again" ) );

            Expect( duplicate.empty(), "Duplicate packet unexpectedly produced a ready command" );
            ExpectEqual( expected.packetId, std::uint16_t { 2 }, "Duplicate packet incorrectly moved the sequence" );
        }

        // Out-of-order arrival: packet 2 is buffered until packet 1 fills the gap,
        // then both are drained by the single call that closes the gap.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            const auto buffered = window.Feed( expected, 2, 0, PacketFlags::None, Bytes( "second" ) );

            Expect( buffered.empty(), "Out-of-order packet was delivered before the gap closed" );
            ExpectEqual( expected.packetId, std::uint16_t { 1 }, "Sequence advanced despite a buffered gap" );

            const auto drained = window.Feed( expected, 1, 0, PacketFlags::None, Bytes( "first" ) );

            ExpectEqual( drained.size(), std::size_t { 2 }, "Filling the gap did not drain the buffered packet" );
            ExpectEqual( drained[0], Bytes( "first" ), "Drained packets were not delivered in order (first)" );
            ExpectEqual( drained[1], Bytes( "second" ), "Drained packets were not delivered in order (second)" );
            ExpectEqual( expected.packetId, std::uint16_t { 3 }, "Sequence did not advance past both drained packets" );
        }

        // Fragmented + compressed reassembly: the QuickLZ "hello" fixture from
        // quick_lz_test.cpp, split across two fragments. Real TeamSpeak marks
        // both the first and the final fragment with the Fragmented flag.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            const auto encoded = Bytes( { 0x04, 0x08, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f } );
            const std::vector<std::byte> firstHalf( encoded.begin(), encoded.begin() + 4 );
            const std::vector<std::byte> secondHalf( encoded.begin() + 4, encoded.end() );

            const auto firstFragment =
                window.Feed( expected, 1, 0, PacketFlags::Fragmented | PacketFlags::Compressed, firstHalf );

            Expect( firstFragment.empty(), "First fragment was delivered before the final fragment arrived" );

            const auto completed = window.Feed( expected, 2, 0, PacketFlags::Fragmented, secondHalf );

            ExpectEqual( completed.size(), std::size_t { 1 }, "Final fragment did not complete reassembly" );
            ExpectEqual( completed.front(), Bytes( "hello" ), "Reassembled/decompressed payload is incorrect" );
        }

        // A packet far outside the receive window is a protocol desync, not
        // something to silently buffer.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            ExpectThrows<std::runtime_error>(
                [&window, &expected]() {
                    (void)window.Feed( expected, 200, 0, PacketFlags::None, Bytes( "too-far" ) );
                },
                "Receive window did not reject a packet far outside the window" );
        }

        // An unfragmented command larger than the maximum command size is rejected.
        {
            CommandReceiveWindow window;
            PacketSequence expected { .packetId = 1, .generationId = 0 };

            const std::vector<std::byte> oversized( protocol::packet_limits::MaxCommandSize + 1 );

            ExpectThrows<std::runtime_error>(
                [&window, &expected, &oversized]() {
                    (void)window.Feed( expected, 1, 0, PacketFlags::None, oversized );
                },
                "Oversized command was not rejected" );
        }
    }

} // namespace ts::test
