#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <protocol/command/parser.hpp>
#include <protocol/connection_statistics.hpp>
#include <protocol/message/set_connection_info.hpp>
#include <protocol/packet/command_fragmenter.hpp>
#include <protocol/packet/limits.hpp>
#include <string_view>

namespace ts::test {

    void RunSetConnectionInfoTests() {
        protocol::ConnectionStatistics::Snapshot statistics;

        statistics.pingMs = 23.5;
        statistics.pingDeviationMs = 1.25;

        statistics.sentSpeech = { .packets = 1, .bytes = 2, .bandwidthLastSecond = 3, .bandwidthLastMinute = 4 };
        statistics.sentKeepalive = { .packets = 5, .bytes = 6, .bandwidthLastSecond = 7, .bandwidthLastMinute = 8 };
        statistics.sentControl = { .packets = 9, .bytes = 10, .bandwidthLastSecond = 11, .bandwidthLastMinute = 12 };

        statistics.receivedSpeech = { .packets = 13, .bytes = 14, .bandwidthLastSecond = 15, .bandwidthLastMinute = 16 };
        statistics.receivedKeepalive = { .packets = 17, .bytes = 18, .bandwidthLastSecond = 19, .bandwidthLastMinute = 20 };
        statistics.receivedControl = { .packets = 21, .bytes = 22, .bandwidthLastSecond = 23, .bandwidthLastMinute = 24 };

        statistics.packetLossSpeech = 0.001;
        statistics.packetLossKeepalive = 0.002;
        statistics.packetLossControl = 0.003;
        statistics.packetLossTotal = 0.004;

        const auto serialized = protocol::SetConnectionInfo( statistics ).Serialize();
        const protocol::Command command = protocol::CommandParser::Parse( serialized );

        ExpectEqual( command.Name(), std::string_view( "setconnectioninfo" ), "Wrong connection-info command name" );
        ExpectEqual( command.Rows().size(), std::size_t { 1 }, "Connection-info command should contain one row" );
        ExpectEqual( command.Rows()[0].Parameters().size(),
                     std::size_t { 30 },
                     "Connection-info command does not contain the declared 30 fields" );

        constexpr std::array<std::string_view, 30> ExpectedFields = {
            "connection_ping",
            "connection_ping_deviation",
            "connection_packets_sent_speech",
            "connection_packets_sent_keepalive",
            "connection_packets_sent_control",
            "connection_bytes_sent_speech",
            "connection_bytes_sent_keepalive",
            "connection_bytes_sent_control",
            "connection_packets_received_speech",
            "connection_packets_received_keepalive",
            "connection_packets_received_control",
            "connection_bytes_received_speech",
            "connection_bytes_received_keepalive",
            "connection_bytes_received_control",
            "connection_server2client_packetloss_speech",
            "connection_server2client_packetloss_keepalive",
            "connection_server2client_packetloss_control",
            "connection_server2client_packetloss_total",
            "connection_bandwidth_sent_last_second_speech",
            "connection_bandwidth_sent_last_second_keepalive",
            "connection_bandwidth_sent_last_second_control",
            "connection_bandwidth_sent_last_minute_speech",
            "connection_bandwidth_sent_last_minute_keepalive",
            "connection_bandwidth_sent_last_minute_control",
            "connection_bandwidth_received_last_second_speech",
            "connection_bandwidth_received_last_second_keepalive",
            "connection_bandwidth_received_last_second_control",
            "connection_bandwidth_received_last_minute_speech",
            "connection_bandwidth_received_last_minute_keepalive",
            "connection_bandwidth_received_last_minute_control",
        };

        const auto& parameters = command.Rows()[0].Parameters();
        for ( std::size_t index = 0; index < ExpectedFields.size(); ++index ) {
            ExpectEqual( parameters[index].Name(), ExpectedFields[index], "Connection-info field order/name is incorrect" );
        }

        ExpectEqual( command.Rows()[0].Require( "connection_ping" ),
                     std::string_view( "23.500000" ),
                     "Connection ping was serialized incorrectly" );
        ExpectEqual( command.Rows()[0].Require( "connection_server2client_packetloss_total" ),
                     std::string_view( "0.004000" ),
                     "Packet loss must be serialized as a fraction, not a percentage" );
        ExpectEqual( command.Rows()[0].Require( "connection_bandwidth_received_last_minute_control" ),
                     std::string_view( "24" ),
                     "Connection bandwidth field was serialized incorrectly" );

        Expect( serialized.size() > protocol::packet_limits::MaxClientPayload,
                "Full setconnectioninfo unexpectedly fits in one datagram; fragmentation path is not exercised" );

        const auto fragments = protocol::CommandFragmenter::Plan( serialized.size() );

        Expect( fragments.size() > 1, "setconnectioninfo was not planned as a fragmented Command" );

        std::size_t covered = 0;
        for ( const auto& fragment : fragments ) {
            ExpectEqual( fragment.offset, covered, "setconnectioninfo fragment offsets are not contiguous" );
            Expect( fragment.size <= protocol::packet_limits::MaxClientPayload,
                    "setconnectioninfo fragment exceeds the TeamSpeak payload limit" );
            covered += fragment.size;
        }

        ExpectEqual( covered, serialized.size(), "setconnectioninfo fragmentation does not cover the command" );
    }

} // namespace ts::test
