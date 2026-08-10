#include "test_support.hpp"

#include <cstddef>
#include <protocol/packet/command_fragmenter.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <stdexcept>

namespace ts::test {

    void RunCommandFragmenterTests() {
        using protocol::CommandFragmenter;
        using protocol::HasFlag;
        using protocol::PacketFlags;

        {
            const auto fragments = CommandFragmenter::Plan( protocol::packet_limits::MaxClientPayload );

            ExpectEqual( fragments.size(), std::size_t { 1 }, "Single-packet command was fragmented" );
            ExpectEqual( fragments[0].offset, std::size_t { 0 }, "Single-packet command offset is incorrect" );
            ExpectEqual( fragments[0].size,
                         protocol::packet_limits::MaxClientPayload,
                         "Single-packet command payload size is incorrect" );
            Expect( HasFlag( fragments[0].flags, PacketFlags::NewProtocol ), "Single-packet Command is missing NewProtocol" );
            Expect( !HasFlag( fragments[0].flags, PacketFlags::Fragmented ),
                    "Single-packet Command unexpectedly has Fragmented" );
        }

        {
            const auto fragments = CommandFragmenter::Plan( protocol::packet_limits::MaxClientPayload + 1 );

            ExpectEqual( fragments.size(), std::size_t { 2 }, "Two-packet command fragment count is incorrect" );
            ExpectEqual( fragments[0].size,
                         protocol::packet_limits::MaxClientPayload,
                         "First command fragment has the wrong size" );
            ExpectEqual( fragments[1].size, std::size_t { 1 }, "Final command fragment has the wrong size" );

            for ( const auto& fragment : fragments ) {
                Expect( HasFlag( fragment.flags, PacketFlags::NewProtocol ), "Command fragment is missing NewProtocol" );
                Expect( HasFlag( fragment.flags, PacketFlags::Fragmented ),
                        "First/final command fragment is missing Fragmented" );
            }
        }

        {
            const auto fragments = CommandFragmenter::Plan( protocol::packet_limits::MaxClientPayload * 2 + 7 );

            ExpectEqual( fragments.size(), std::size_t { 3 }, "Three-packet command fragment count is incorrect" );
            ExpectEqual( fragments[0].offset, std::size_t { 0 }, "First fragment offset is incorrect" );
            ExpectEqual( fragments[1].offset,
                         protocol::packet_limits::MaxClientPayload,
                         "Middle fragment offset is incorrect" );
            ExpectEqual( fragments[2].offset,
                         protocol::packet_limits::MaxClientPayload * 2,
                         "Final fragment offset is incorrect" );
            ExpectEqual( fragments[2].size, std::size_t { 7 }, "Final fragment size is incorrect" );

            Expect( HasFlag( fragments[0].flags, PacketFlags::Fragmented ), "First fragment is missing Fragmented" );
            Expect( !HasFlag( fragments[1].flags, PacketFlags::Fragmented ), "Middle fragment unexpectedly has Fragmented" );
            Expect( HasFlag( fragments[2].flags, PacketFlags::Fragmented ), "Final fragment is missing Fragmented" );

            for ( const auto& fragment : fragments ) {
                Expect( HasFlag( fragment.flags, PacketFlags::NewProtocol ), "Split command fragment is missing NewProtocol" );
                Expect( fragment.size <= protocol::packet_limits::MaxClientPayload,
                        "Command fragment exceeds the TeamSpeak client payload limit" );
            }
        }

        ExpectThrows<std::runtime_error>(
            []() {
                (void)CommandFragmenter::Plan( protocol::packet_limits::MaxCommandSize + 1 );
            },
            "Oversized command was accepted by the fragment planner" );
    }

} // namespace ts::test
