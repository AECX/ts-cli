#ifndef TS_PROTOCOL_RELIABILITY_RELIABLE_COMMAND_QUEUE_HPP
#define TS_PROTOCOL_RELIABILITY_RELIABLE_COMMAND_QUEUE_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <protocol/packet/packet.hpp>
#include <vector>

namespace ts::protocol {

    class ReliableCommandQueue {
      public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        void Add( std::uint16_t packetId, std::uint32_t generationId, Packet packet, TimePoint sentAt );

        struct Acknowledgement {
            TimePoint firstSent;
            bool retransmitted = false;
        };

        [[nodiscard]] std::optional<Acknowledgement> Acknowledge( std::uint16_t packetId );

        [[nodiscard]] std::vector<Packet> CollectDue( TimePoint now );
        [[nodiscard]] std::optional<TimePoint> NextDeadline() const;
        [[nodiscard]] bool Contains( std::uint16_t packetId ) const;
        [[nodiscard]] std::size_t Size() const;

      private:
        struct PendingCommand {
            std::uint16_t packetId = 0;
            std::uint32_t generationId = 0;

            Packet packet;

            TimePoint firstSent;
            TimePoint nextRetry;

            std::chrono::milliseconds retryDelay { 0 };
            bool retransmitted = false;
        };

        static constexpr std::size_t MaxPendingCommands = 1024;

        static constexpr std::chrono::milliseconds InitialRetryDelay { 500 };
        static constexpr std::chrono::milliseconds MaxRetryDelay { 4000 };
        static constexpr std::chrono::seconds CommandTimeout { 30 };

        [[nodiscard]] static std::uint64_t SequenceKey( std::uint16_t packetId, std::uint32_t generationId );

        std::map<std::uint64_t, PendingCommand> m_Pending;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_RELIABILITY_RELIABLE_COMMAND_QUEUE_HPP
