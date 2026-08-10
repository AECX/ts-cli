#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    void ReliableCommandQueue::Add( std::uint16_t packetId, std::uint32_t generationId, Packet packet, TimePoint sentAt ) {
        if ( m_Pending.size() >= MaxPendingCommands ) {
            throw std::runtime_error( "Reliable Command send queue is full" );
        }

        for ( const auto& [key, pending] : m_Pending ) {
            (void)key;

            if ( pending.packetId == packetId ) {
                throw std::runtime_error( "Reliable Command packet ID wrapped while still pending" );
            }
        }

        const std::uint64_t key = SequenceKey( packetId, generationId );

        const auto [iterator, inserted] = m_Pending.emplace( key,
                                                             PendingCommand { .packetId = packetId,
                                                                              .generationId = generationId,
                                                                              .packet = std::move( packet ),
                                                                              .firstSent = sentAt,
                                                                              .nextRetry = sentAt + InitialRetryDelay,
                                                                              .retryDelay = InitialRetryDelay } );

        (void)iterator;

        if ( !inserted ) {
            throw std::runtime_error( "Reliable Command sequence is already pending" );
        }
    }

    std::optional<ReliableCommandQueue::Acknowledgement> ReliableCommandQueue::Acknowledge( std::uint16_t packetId ) {
        auto match = m_Pending.end();

        for ( auto iterator = m_Pending.begin(); iterator != m_Pending.end(); ++iterator ) {
            if ( iterator->second.packetId != packetId ) {
                continue;
            }

            if ( match != m_Pending.end() ) {
                throw std::runtime_error( "Ambiguous reliable Command acknowledgement" );
            }

            match = iterator;
        }

        if ( match == m_Pending.end() ) {
            return std::nullopt;
        }

        const Acknowledgement acknowledgement { .firstSent = match->second.firstSent,
                                                .retransmitted = match->second.retransmitted };

        m_Pending.erase( match );

        return acknowledgement;
    }

    std::vector<Packet> ReliableCommandQueue::CollectDue( TimePoint now ) {
        std::vector<Packet> result;

        for ( auto& [key, pending] : m_Pending ) {
            (void)key;

            if ( now >= pending.firstSent + CommandTimeout ) {
                throw std::runtime_error( "Reliable Command acknowledgement timed out" );
            }

            if ( now < pending.nextRetry ) {
                continue;
            }

            result.push_back( pending.packet );
            pending.retransmitted = true;

            pending.retryDelay = std::min( pending.retryDelay * 2, MaxRetryDelay );
            pending.nextRetry = now + pending.retryDelay;
        }

        return result;
    }

    std::optional<ReliableCommandQueue::TimePoint> ReliableCommandQueue::NextDeadline() const {
        std::optional<TimePoint> result;

        for ( const auto& [key, pending] : m_Pending ) {
            (void)key;

            const TimePoint timeout = pending.firstSent + CommandTimeout;
            const TimePoint deadline = std::min( pending.nextRetry, timeout );

            if ( !result || deadline < *result ) {
                result = deadline;
            }
        }

        return result;
    }

    bool ReliableCommandQueue::Contains( std::uint16_t packetId ) const {
        for ( const auto& [key, pending] : m_Pending ) {
            (void)key;

            if ( pending.packetId == packetId ) {
                return true;
            }
        }

        return false;
    }

    std::size_t ReliableCommandQueue::Size() const {
        return m_Pending.size();
    }

    std::uint64_t ReliableCommandQueue::SequenceKey( std::uint16_t packetId, std::uint32_t generationId ) {
        return ( static_cast<std::uint64_t>( generationId ) << 16 ) | static_cast<std::uint64_t>( packetId );
    }

} // namespace ts::protocol
