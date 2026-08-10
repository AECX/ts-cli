#include <cstddef>
#include <protocol/command/writer.hpp>
#include <protocol/message/set_connection_info.hpp>
#include <string>
#include <utility>
#include <vector>

namespace ts::protocol {

    SetConnectionInfo::SetConnectionInfo( ConnectionStatistics::Snapshot statistics ): m_Statistics( std::move( statistics ) ) {
    }

    std::vector<std::byte> SetConnectionInfo::Serialize() const {
        CommandWriter writer( "setconnectioninfo" );

        writer.Write( "connection_ping", std::to_string( m_Statistics.pingMs ) );
        writer.Write( "connection_ping_deviation", std::to_string( m_Statistics.pingDeviationMs ) );

        writer.Write( "connection_packets_sent_speech", m_Statistics.sentSpeech.packets );
        writer.Write( "connection_packets_sent_keepalive", m_Statistics.sentKeepalive.packets );
        writer.Write( "connection_packets_sent_control", m_Statistics.sentControl.packets );
        writer.Write( "connection_bytes_sent_speech", m_Statistics.sentSpeech.bytes );
        writer.Write( "connection_bytes_sent_keepalive", m_Statistics.sentKeepalive.bytes );
        writer.Write( "connection_bytes_sent_control", m_Statistics.sentControl.bytes );

        writer.Write( "connection_packets_received_speech", m_Statistics.receivedSpeech.packets );
        writer.Write( "connection_packets_received_keepalive", m_Statistics.receivedKeepalive.packets );
        writer.Write( "connection_packets_received_control", m_Statistics.receivedControl.packets );
        writer.Write( "connection_bytes_received_speech", m_Statistics.receivedSpeech.bytes );
        writer.Write( "connection_bytes_received_keepalive", m_Statistics.receivedKeepalive.bytes );
        writer.Write( "connection_bytes_received_control", m_Statistics.receivedControl.bytes );

        writer.Write( "connection_server2client_packetloss_speech", std::to_string( m_Statistics.packetLossSpeech ) );
        writer.Write( "connection_server2client_packetloss_keepalive", std::to_string( m_Statistics.packetLossKeepalive ) );
        writer.Write( "connection_server2client_packetloss_control", std::to_string( m_Statistics.packetLossControl ) );
        writer.Write( "connection_server2client_packetloss_total", std::to_string( m_Statistics.packetLossTotal ) );

        writer.Write( "connection_bandwidth_sent_last_second_speech", m_Statistics.sentSpeech.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_sent_last_second_keepalive", m_Statistics.sentKeepalive.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_sent_last_second_control", m_Statistics.sentControl.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_sent_last_minute_speech", m_Statistics.sentSpeech.bandwidthLastMinute );
        writer.Write( "connection_bandwidth_sent_last_minute_keepalive", m_Statistics.sentKeepalive.bandwidthLastMinute );
        writer.Write( "connection_bandwidth_sent_last_minute_control", m_Statistics.sentControl.bandwidthLastMinute );

        writer.Write( "connection_bandwidth_received_last_second_speech", m_Statistics.receivedSpeech.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_received_last_second_keepalive",
                      m_Statistics.receivedKeepalive.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_received_last_second_control", m_Statistics.receivedControl.bandwidthLastSecond );
        writer.Write( "connection_bandwidth_received_last_minute_speech", m_Statistics.receivedSpeech.bandwidthLastMinute );
        writer.Write( "connection_bandwidth_received_last_minute_keepalive",
                      m_Statistics.receivedKeepalive.bandwidthLastMinute );
        writer.Write( "connection_bandwidth_received_last_minute_control", m_Statistics.receivedControl.bandwidthLastMinute );

        return writer.Take();
    }

} // namespace ts::protocol
