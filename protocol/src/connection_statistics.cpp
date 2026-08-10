#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <protocol/connection_statistics.hpp>
#include <stdexcept>
#include <utility>

namespace ts::protocol {

    void ConnectionStatistics::Reset( TimePoint now ) {
        m_Sent = {};
        m_Received = {};
        m_Loss = {};

        for ( TrafficWindow& window : m_SentWindows ) {
            window.Reset();
        }

        for ( TrafficWindow& window : m_ReceivedWindows ) {
            window.Reset();
        }

        for ( LossWindow& window : m_LossWindows ) {
            window.Reset();
        }

        m_StartedAt = now;
        m_HasRtt = false;
        m_PingMs = 0.0;
        m_PingDeviationMs = 0.0;
    }

    ConnectionStatistics::LossDelta ConnectionStatistics::LossTracker::Observe( std::uint16_t packetId ) {
        const std::uint64_t expectedBefore = expected;
        const std::uint64_t lostBefore = Lost();

        const auto result = [&]() -> LossDelta {
            const std::uint64_t lostAfter = Lost();

            std::int64_t lostDelta = 0;
            if ( lostAfter >= lostBefore ) {
                lostDelta = static_cast<std::int64_t>( lostAfter - lostBefore );
            } else {
                lostDelta = -static_cast<std::int64_t>( lostBefore - lostAfter );
            }

            return LossDelta {
                .expected = expected - expectedBefore,
                .lost = lostDelta,
            };
        };

        if ( !initialized ) {
            initialized = true;
            highestSequence = packetId;
            expected = 1;
            return result();
        }

        const auto highestPacketId = static_cast<std::uint16_t>( highestSequence & 0xffffU );
        const std::uint16_t forwardDistance = static_cast<std::uint16_t>( packetId - highestPacketId );

        if ( forwardDistance == 0 ) {
            return result();
        }

        if ( forwardDistance < 0x8000U ) {
            const std::uint64_t previousHighest = highestSequence;
            highestSequence += forwardDistance;
            expected += forwardDistance;

            const std::uint64_t gap = static_cast<std::uint64_t>( forwardDistance - 1U );
            const std::uint64_t retainedGap = std::min( gap, ReorderRetention );

            confirmedLost += gap - retainedGap;

            const std::uint64_t firstRetained = highestSequence - retainedGap;
            for ( std::uint64_t sequence = firstRetained; sequence < highestSequence; ++sequence ) {
                if ( sequence > previousHighest ) {
                    missing.insert( sequence );
                }
            }

            const std::uint64_t oldestRetained = highestSequence > ReorderRetention ? highestSequence - ReorderRetention : 0;

            while ( !missing.empty() && *missing.begin() < oldestRetained ) {
                ++confirmedLost;
                missing.erase( missing.begin() );
            }

            return result();
        }

        const std::uint16_t backwardDistance = static_cast<std::uint16_t>( highestPacketId - packetId );

        if ( static_cast<std::uint64_t>( backwardDistance ) > highestSequence ) {
            return result();
        }

        const std::uint64_t sequence = highestSequence - backwardDistance;
        missing.erase( sequence );

        return result();
    }

    std::uint64_t ConnectionStatistics::LossTracker::Lost() const {
        return confirmedLost + static_cast<std::uint64_t>( missing.size() );
    }

    void ConnectionStatistics::LossWindow::Reset() {
        samples.clear();
    }

    void ConnectionStatistics::LossWindow::Add( TimePoint now, LossDelta delta ) {
        constexpr auto Retention = std::chrono::seconds( 2 );

        while ( !samples.empty() && now - samples.front().time > Retention ) {
            samples.pop_front();
        }

        if ( delta.expected == 0 && delta.lost == 0 ) {
            return;
        }

        samples.push_back( Sample {
            .time = now,
            .expected = delta.expected,
            .lost = delta.lost,
        } );
    }

    std::pair<std::uint64_t, std::uint64_t> ConnectionStatistics::LossWindow::Totals( TimePoint now,
                                                                                      std::chrono::seconds window ) const {
        const TimePoint start = now - window;

        std::uint64_t expected = 0;
        std::int64_t lost = 0;

        for ( const Sample& sample : samples ) {
            if ( sample.time < start || sample.time > now ) {
                continue;
            }

            expected += sample.expected;
            lost += sample.lost;
        }

        if ( lost <= 0 || expected == 0 ) {
            return { 0, expected };
        }

        const auto lostUnsigned = static_cast<std::uint64_t>( lost );
        return { std::min( lostUnsigned, expected ), expected };
    }

    void ConnectionStatistics::TrafficWindow::Reset() {
        samples.clear();
    }

    void ConnectionStatistics::TrafficWindow::Add( TimePoint now, std::size_t packetBytes ) {
        constexpr auto Retention = std::chrono::minutes( 1 );

        while ( !samples.empty() && now - samples.front().time > Retention ) {
            samples.pop_front();
        }

        samples.push_back( Sample { .time = now, .bytes = static_cast<std::uint64_t>( packetBytes ) } );
    }

    std::uint64_t ConnectionStatistics::TrafficWindow::Rate( TimePoint now,
                                                             TimePoint statisticsStartedAt,
                                                             std::chrono::seconds window ) const {
        if ( now <= statisticsStartedAt ) {
            return 0;
        }

        const TimePoint requestedStart = now - window;
        const TimePoint effectiveStart = std::max( requestedStart, statisticsStartedAt );

        std::uint64_t bytes = 0;
        for ( const Sample& sample : samples ) {
            if ( sample.time >= effectiveStart && sample.time <= now ) {
                bytes += sample.bytes;
            }
        }

        auto elapsed = now - effectiveStart;
        const auto minimumElapsed = std::chrono::seconds( 1 );
        if ( elapsed < minimumElapsed ) {
            elapsed = minimumElapsed;
        }

        const double seconds = std::chrono::duration<double>( elapsed ).count();
        const double rate = static_cast<double>( bytes ) / seconds;

        if ( rate <= 0.0 ) {
            return 0;
        }

        const double maximum = static_cast<double>( std::numeric_limits<std::uint64_t>::max() );
        if ( rate >= maximum ) {
            return std::numeric_limits<std::uint64_t>::max();
        }

        return static_cast<std::uint64_t>( std::llround( rate ) );
    }

    std::optional<ConnectionStatistics::TrafficClass> ConnectionStatistics::Classify( PacketType type ) {
        switch ( type ) {
            case PacketType::Voice:
            case PacketType::VoiceWhisper:
                return TrafficClass::Speech;

            case PacketType::Ping:
            case PacketType::Pong:
                return TrafficClass::Keepalive;

            case PacketType::Command:
            case PacketType::CommandLow:
            case PacketType::Ack:
            case PacketType::AckLow:
                return TrafficClass::Control;

            case PacketType::Init1:
                return std::nullopt;
        }

        return std::nullopt;
    }

    bool ConnectionStatistics::TracksIncomingLoss( PacketType type ) {
        switch ( type ) {
            case PacketType::Voice:
            case PacketType::VoiceWhisper:
            case PacketType::Ping:
            case PacketType::Pong:
            case PacketType::Ack:
            case PacketType::AckLow:
                return true;

            case PacketType::Command:
            case PacketType::CommandLow:
            case PacketType::Init1:
                return false;
        }

        return false;
    }

    std::size_t ConnectionStatistics::TypeIndex( PacketType type ) {
        const auto value = static_cast<std::uint8_t>( type );

        if ( static_cast<std::size_t>( value ) >= PacketTypeCount ) {
            throw std::runtime_error( "Invalid packet type for connection statistics" );
        }

        return static_cast<std::size_t>( value );
    }

    void ConnectionStatistics::RecordSent( PacketType type, std::size_t packetBytes, TimePoint now ) {
        const auto trafficClass = Classify( type );

        if ( !trafficClass ) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>( *trafficClass );
        TrafficCounters& counters = m_Sent[index];

        ++counters.packets;
        counters.bytes += static_cast<std::uint64_t>( packetBytes );
        m_SentWindows[index].Add( now, packetBytes );
    }

    void ConnectionStatistics::RecordReceived( PacketType type,
                                               std::uint16_t packetId,
                                               std::size_t packetBytes,
                                               TimePoint now ) {
        const auto trafficClass = Classify( type );

        if ( !trafficClass ) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>( *trafficClass );
        TrafficCounters& counters = m_Received[index];

        ++counters.packets;
        counters.bytes += static_cast<std::uint64_t>( packetBytes );
        m_ReceivedWindows[index].Add( now, packetBytes );

        if ( TracksIncomingLoss( type ) ) {
            const std::size_t typeIndex = TypeIndex( type );
            const LossDelta delta = m_Loss[typeIndex].Observe( packetId );
            m_LossWindows[typeIndex].Add( now, delta );
        }
    }

    void ConnectionStatistics::RecordRtt( Clock::duration rtt ) {
        const double sampleMs = std::chrono::duration<double, std::milli>( rtt ).count();

        if ( sampleMs < 0.0 ) {
            return;
        }

        if ( !m_HasRtt ) {
            m_HasRtt = true;
            m_PingMs = sampleMs;
            m_PingDeviationMs = sampleMs / 2.0;
            return;
        }

        constexpr double Alpha = 1.0 / 8.0;
        constexpr double Beta = 1.0 / 4.0;

        const double difference = std::abs( m_PingMs - sampleMs );

        m_PingDeviationMs = ( 1.0 - Beta ) * m_PingDeviationMs + Beta * difference;
        m_PingMs = ( 1.0 - Alpha ) * m_PingMs + Alpha * sampleMs;
    }

    double ConnectionStatistics::LossFraction( std::uint64_t lost, std::uint64_t expected ) {
        if ( expected == 0 ) {
            return 0.0;
        }

        return static_cast<double>( lost ) / static_cast<double>( expected );
    }

    std::pair<std::uint64_t, std::uint64_t> ConnectionStatistics::LossTotals( TrafficClass trafficClass, TimePoint now ) const {
        constexpr auto Window = std::chrono::seconds( 1 );

        std::uint64_t lost = 0;
        std::uint64_t expected = 0;

        for ( std::size_t index = 0; index < PacketTypeCount; ++index ) {
            const auto type = static_cast<PacketType>( static_cast<std::uint8_t>( index ) );
            const auto classified = Classify( type );

            if ( !classified || *classified != trafficClass || !TracksIncomingLoss( type ) ) {
                continue;
            }

            const auto totals = m_LossWindows[index].Totals( now, Window );
            lost += totals.first;
            expected += totals.second;
        }

        return { lost, expected };
    }

    ConnectionStatistics::Snapshot ConnectionStatistics::GetSnapshot( TimePoint now ) const {
        Snapshot snapshot;

        snapshot.pingMs = m_PingMs;
        snapshot.pingDeviationMs = m_PingDeviationMs;

        snapshot.sentSpeech = m_Sent[static_cast<std::size_t>( TrafficClass::Speech )];
        snapshot.sentKeepalive = m_Sent[static_cast<std::size_t>( TrafficClass::Keepalive )];
        snapshot.sentControl = m_Sent[static_cast<std::size_t>( TrafficClass::Control )];

        snapshot.receivedSpeech = m_Received[static_cast<std::size_t>( TrafficClass::Speech )];
        snapshot.receivedKeepalive = m_Received[static_cast<std::size_t>( TrafficClass::Keepalive )];
        snapshot.receivedControl = m_Received[static_cast<std::size_t>( TrafficClass::Control )];

        constexpr auto Second = std::chrono::seconds( 1 );
        constexpr auto Minute = std::chrono::minutes( 1 );

        for ( std::size_t index = 0; index < static_cast<std::size_t>( TrafficClass::Count ); ++index ) {
            const std::uint64_t sentSecond = m_SentWindows[index].Rate( now, m_StartedAt, Second );
            const std::uint64_t sentMinute = m_SentWindows[index].Rate( now, m_StartedAt, Minute );
            const std::uint64_t receivedSecond = m_ReceivedWindows[index].Rate( now, m_StartedAt, Second );
            const std::uint64_t receivedMinute = m_ReceivedWindows[index].Rate( now, m_StartedAt, Minute );

            TrafficCounters* sent = nullptr;
            TrafficCounters* received = nullptr;

            switch ( static_cast<TrafficClass>( index ) ) {
                case TrafficClass::Speech:
                    sent = &snapshot.sentSpeech;
                    received = &snapshot.receivedSpeech;
                    break;
                case TrafficClass::Keepalive:
                    sent = &snapshot.sentKeepalive;
                    received = &snapshot.receivedKeepalive;
                    break;
                case TrafficClass::Control:
                    sent = &snapshot.sentControl;
                    received = &snapshot.receivedControl;
                    break;
                case TrafficClass::Count:
                    break;
            }

            if ( sent == nullptr || received == nullptr ) {
                continue;
            }

            sent->bandwidthLastSecond = sentSecond;
            sent->bandwidthLastMinute = sentMinute;
            received->bandwidthLastSecond = receivedSecond;
            received->bandwidthLastMinute = receivedMinute;
        }

        const auto speech = LossTotals( TrafficClass::Speech, now );
        const auto keepalive = LossTotals( TrafficClass::Keepalive, now );
        const auto control = LossTotals( TrafficClass::Control, now );

        snapshot.packetLossSpeech = LossFraction( speech.first, speech.second );
        snapshot.packetLossKeepalive = LossFraction( keepalive.first, keepalive.second );
        snapshot.packetLossControl = LossFraction( control.first, control.second );

        const std::uint64_t totalLost = speech.first + keepalive.first + control.first;
        const std::uint64_t totalExpected = speech.second + keepalive.second + control.second;

        snapshot.packetLossTotal = LossFraction( totalLost, totalExpected );

        return snapshot;
    }

} // namespace ts::protocol
