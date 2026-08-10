#include "test_support.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <protocol/connection_statistics.hpp>
#include <protocol/packet/packet_type.hpp>

namespace ts::test {

    void RunConnectionStatisticsTests() {
        using namespace std::chrono_literals;

        using Statistics = protocol::ConnectionStatistics;

        const Statistics::TimePoint start {};
        Statistics statistics;
        statistics.Reset( start );

        statistics.RecordSent( protocol::PacketType::Command, 100, start + 100ms );
        statistics.RecordSent( protocol::PacketType::Ping, 13, start + 100ms );
        statistics.RecordSent( protocol::PacketType::Voice, 200, start + 100ms );

        statistics.RecordReceived( protocol::PacketType::Command, 10, 90, start + 100ms );
        statistics.RecordReceived( protocol::PacketType::Command, 50, 90, start + 150ms );
        statistics.RecordReceived( protocol::PacketType::Ping, 65534, 11, start + 100ms );
        statistics.RecordReceived( protocol::PacketType::Ping, 65535, 11, start + 200ms );
        statistics.RecordReceived( protocol::PacketType::Ping, 1, 11, start + 300ms );

        {
            const auto snapshot = statistics.GetSnapshot( start + 1s );

            ExpectEqual( snapshot.sentControl.packets, std::uint64_t { 1 }, "Control send packet count is incorrect" );
            ExpectEqual( snapshot.sentKeepalive.packets, std::uint64_t { 1 }, "Keepalive send packet count is incorrect" );
            ExpectEqual( snapshot.sentSpeech.packets, std::uint64_t { 1 }, "Speech send packet count is incorrect" );
            ExpectEqual( snapshot.receivedControl.bytes, std::uint64_t { 180 }, "Control receive byte count is incorrect" );

            ExpectEqual( snapshot.sentControl.bandwidthLastSecond,
                         std::uint64_t { 100 },
                         "Last-second sent bandwidth is incorrect" );
            ExpectEqual( snapshot.receivedControl.bandwidthLastSecond,
                         std::uint64_t { 180 },
                         "Last-second received bandwidth is incorrect" );

            Expect( std::abs( snapshot.packetLossKeepalive - 0.25 ) < 0.000001,
                    "Packet loss across a packet-id wrap was calculated incorrectly" );
            Expect( std::abs( snapshot.packetLossControl ) < 0.000001,
                    "Reliable Command sequence gaps must not be reported as incoming control loss" );
        }

        statistics.RecordReceived( protocol::PacketType::Ack, 100, 11, start + 1050ms );
        statistics.RecordReceived( protocol::PacketType::Ack, 102, 11, start + 1060ms );

        {
            const auto snapshot = statistics.GetSnapshot( start + 1100ms );

            Expect( std::abs( snapshot.packetLossControl - ( 1.0 / 3.0 ) ) < 0.000001,
                    "Ack sequence gaps are not reflected in incoming control loss" );
        }

        statistics.RecordReceived( protocol::PacketType::Ack, 101, 11, start + 1150ms );
        statistics.RecordReceived( protocol::PacketType::Ping, 0, 11, start + 1100ms );
        statistics.RecordReceived( protocol::PacketType::Ping, 0, 11, start + 1200ms );

        {
            const auto snapshot = statistics.GetSnapshot( start + 2s );

            Expect( std::abs( snapshot.packetLossKeepalive ) < 0.000001, "Late packet did not heal a temporary sequence gap" );
            Expect( std::abs( snapshot.packetLossControl ) < 0.000001, "Late Ack did not heal a temporary control-loss gap" );
            ExpectEqual( snapshot.sentControl.bandwidthLastSecond,
                         std::uint64_t { 0 },
                         "Expired traffic remained in the last-second bandwidth window" );
            ExpectEqual( snapshot.sentControl.bandwidthLastMinute,
                         std::uint64_t { 50 },
                         "Last-minute bandwidth is not normalized over elapsed connection time" );
        }

        statistics.RecordRtt( 40ms );

        {
            const auto snapshot = statistics.GetSnapshot( start + 2s );

            Expect( std::abs( snapshot.pingMs - 40.0 ) < 0.000001, "First RTT sample is incorrect" );
            Expect( std::abs( snapshot.pingDeviationMs - 20.0 ) < 0.000001, "Initial RTT deviation is incorrect" );
        }

        statistics.RecordRtt( 56ms );

        {
            const auto snapshot = statistics.GetSnapshot( start + 2s );

            Expect( std::abs( snapshot.pingMs - 42.0 ) < 0.000001, "Smoothed RTT is incorrect" );
            Expect( std::abs( snapshot.pingDeviationMs - 19.0 ) < 0.000001, "Smoothed RTT deviation is incorrect" );
        }

        statistics.Reset( start + 3s );

        {
            const auto snapshot = statistics.GetSnapshot( start + 4s );

            ExpectEqual( snapshot.sentControl.packets, std::uint64_t { 0 }, "Reset retained sent packet counters" );
            Expect( std::abs( snapshot.pingMs ) < 0.000001, "Reset retained RTT state" );
            Expect( std::abs( snapshot.packetLossTotal ) < 0.000001, "Reset retained packet-loss state" );
        }
    }

} // namespace ts::test
