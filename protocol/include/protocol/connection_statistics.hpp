#ifndef TS_PROTOCOL_CONNECTION_STATISTICS_HPP
#define TS_PROTOCOL_CONNECTION_STATISTICS_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <protocol/packet/packet_type.hpp>
#include <set>
#include <utility>

namespace ts::protocol {

    class ConnectionStatistics {
      public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        struct TrafficCounters {
            std::uint64_t packets = 0;
            std::uint64_t bytes = 0;
            std::uint64_t bandwidthLastSecond = 0;
            std::uint64_t bandwidthLastMinute = 0;
        };

        struct Snapshot {
            double pingMs = 0.0;
            double pingDeviationMs = 0.0;

            TrafficCounters sentSpeech;
            TrafficCounters sentKeepalive;
            TrafficCounters sentControl;

            TrafficCounters receivedSpeech;
            TrafficCounters receivedKeepalive;
            TrafficCounters receivedControl;

            double packetLossSpeech = 0.0;
            double packetLossKeepalive = 0.0;
            double packetLossControl = 0.0;
            double packetLossTotal = 0.0;
        };

        void Reset( TimePoint now = Clock::now() );

        void RecordSent( PacketType type, std::size_t packetBytes, TimePoint now = Clock::now() );
        void RecordReceived( PacketType type, std::uint16_t packetId, std::size_t packetBytes, TimePoint now = Clock::now() );
        void RecordRtt( Clock::duration rtt );

        [[nodiscard]] Snapshot GetSnapshot( TimePoint now = Clock::now() ) const;

      private:
        enum class TrafficClass : std::size_t { Speech = 0, Keepalive = 1, Control = 2, Count = 3 };

        struct LossDelta {
            std::uint64_t expected = 0;
            std::int64_t lost = 0;
        };

        struct LossTracker {
            static constexpr std::uint64_t ReorderRetention = 4096;

            bool initialized = false;
            std::uint64_t highestSequence = 0;
            std::uint64_t expected = 0;
            std::uint64_t confirmedLost = 0;
            std::set<std::uint64_t> missing;

            [[nodiscard]] LossDelta Observe( std::uint16_t packetId );
            [[nodiscard]] std::uint64_t Lost() const;
        };

        struct LossWindow {
            struct Sample {
                TimePoint time;
                std::uint64_t expected = 0;
                std::int64_t lost = 0;
            };

            std::deque<Sample> samples;

            void Reset();
            void Add( TimePoint now, LossDelta delta );

            [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> Totals( TimePoint now, std::chrono::seconds window ) const;
        };

        struct TrafficWindow {
            struct Sample {
                TimePoint time;
                std::uint64_t bytes = 0;
            };

            std::deque<Sample> samples;

            void Reset();
            void Add( TimePoint now, std::size_t packetBytes );

            [[nodiscard]] std::uint64_t Rate( TimePoint now, TimePoint statisticsStartedAt, std::chrono::seconds window ) const;
        };

        static constexpr std::size_t PacketTypeCount = 9;

        [[nodiscard]] static std::optional<TrafficClass> Classify( PacketType type );
        [[nodiscard]] static bool TracksIncomingLoss( PacketType type );
        [[nodiscard]] static std::size_t TypeIndex( PacketType type );
        [[nodiscard]] static double LossFraction( std::uint64_t lost, std::uint64_t expected );

        [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> LossTotals( TrafficClass trafficClass, TimePoint now ) const;

        std::array<TrafficCounters, static_cast<std::size_t>( TrafficClass::Count )> m_Sent {};
        std::array<TrafficCounters, static_cast<std::size_t>( TrafficClass::Count )> m_Received {};
        std::array<TrafficWindow, static_cast<std::size_t>( TrafficClass::Count )> m_SentWindows {};
        std::array<TrafficWindow, static_cast<std::size_t>( TrafficClass::Count )> m_ReceivedWindows {};
        std::array<LossTracker, PacketTypeCount> m_Loss {};
        std::array<LossWindow, PacketTypeCount> m_LossWindows {};

        TimePoint m_StartedAt = Clock::now();

        bool m_HasRtt = false;
        double m_PingMs = 0.0;
        double m_PingDeviationMs = 0.0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CONNECTION_STATISTICS_HPP
