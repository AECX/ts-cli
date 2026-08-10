#include "test_support.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <protocol/packet/packet.hpp>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <stdexcept>
#include <vector>

namespace ts::test {

    void RunReliableCommandQueueTests() {
        using namespace std::chrono_literals;

        using Queue = protocol::ReliableCommandQueue;

        const Queue::TimePoint start {};

        {
            Queue queue;

            const auto payload = Bytes( { 0x10, 0x20, 0x30 } );

            queue.Add( 7, 0, protocol::Packet( payload ), start );

            ExpectEqual( queue.Size(), std::size_t { 1 }, "Reliable queue did not retain the sent Command" );

            Expect( queue.Contains( 7 ), "Reliable queue could not find a pending Command" );

            Expect( !queue.Contains( 8 ), "Reliable queue reported an unrelated Command as pending" );

            const auto deadline = queue.NextDeadline();

            Expect( deadline.has_value(), "Reliable queue did not expose a retry deadline" );

            ExpectEqual( *deadline, start + 500ms, "Reliable queue has the wrong initial retry deadline" );

            const auto early = queue.CollectDue( start + 499ms );

            Expect( early.empty(), "Reliable Command was retried too early" );

            const auto due = queue.CollectDue( start + 500ms );

            ExpectEqual( due.size(), std::size_t { 1 }, "Reliable Command was not retried at its deadline" );

            const std::vector<std::byte> resent( due.front().Data().begin(), due.front().Data().end() );

            ExpectEqual( resent, payload, "Reliable retry did not preserve the exact packet bytes" );

            const auto secondDeadline = queue.NextDeadline();

            Expect( secondDeadline.has_value(), "Reliable queue lost its second retry deadline" );

            ExpectEqual( *secondDeadline, start + 1500ms, "Reliable retry backoff did not double" );

            const auto acknowledgement = queue.Acknowledge( 7 );

            Expect( acknowledgement.has_value(), "Reliable acknowledgement metadata was not returned" );
            Expect( acknowledgement->retransmitted, "Reliable acknowledgement did not report a retransmission" );
            ExpectEqual( acknowledgement->firstSent, start, "Reliable acknowledgement lost the first-send timestamp" );
        }

        {
            Queue queue;

            queue.Add( 42, 3, protocol::Packet( Bytes( { 0xaa } ) ), start );

            Expect( queue.Contains( 42 ), "Reliable queue lost a Command before acknowledgement" );

            const auto acknowledgement = queue.Acknowledge( 42 );

            Expect( acknowledgement.has_value(), "Reliable acknowledgement metadata was not returned" );
            Expect( !acknowledgement->retransmitted, "Fresh reliable acknowledgement was incorrectly marked retransmitted" );
            ExpectEqual( acknowledgement->firstSent, start, "Reliable acknowledgement lost the first-send timestamp" );

            ExpectEqual( queue.Size(), std::size_t { 0 }, "Reliable acknowledgement did not retire the Command" );

            Expect( !queue.Contains( 42 ), "Reliable queue still reported an acknowledged Command as pending" );

            const auto duplicate = queue.Acknowledge( 42 );

            Expect( !duplicate.has_value(), "Duplicate reliable acknowledgement unexpectedly matched a Command" );

            ExpectEqual( queue.Size(), std::size_t { 0 }, "Duplicate reliable acknowledgement changed queue state" );
        }

        {
            Queue queue;

            queue.Add( 9, 0, protocol::Packet( Bytes( { 0x01 } ) ), start );

            ExpectThrows<std::runtime_error>(
                [&queue, start]() {
                    queue.Add( 9, 1, protocol::Packet( Bytes( { 0x02 } ) ), start );
                },
                "Reliable queue accepted an ambiguous wrapped packet ID" );
        }

        {
            Queue queue;

            queue.Add( 1, 0, protocol::Packet( Bytes( { 0xff } ) ), start );

            ExpectThrows<std::runtime_error>(
                [&queue, start]() {
                    (void)queue.CollectDue( start + 30s );
                },
                "Reliable Command did not time out after 30 seconds" );
        }
    }

} // namespace ts::test
