#include <protocol/compression/quick_lz.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/reliability/command_receive_window.hpp>
#include <stdexcept>
#include <utility>

namespace ts::protocol {

    std::vector<std::vector<std::byte>> CommandReceiveWindow::Feed( PacketSequence& expected,
                                                                    std::uint16_t packetId,
                                                                    std::uint32_t generationId,
                                                                    PacketFlags flags,
                                                                    std::vector<std::byte> plaintext ) {
        std::vector<std::vector<std::byte>> ready;

        const std::uint64_t packetKey = SequenceKey( packetId, generationId );
        const std::uint64_t expectedKey = SequenceKey( expected );

        if ( packetKey < expectedKey ) {
            return ready;
        }

        if ( packetKey > expectedKey ) {
            const std::uint64_t distance = packetKey - expectedKey;

            if ( distance > MaxQueuedPackets ) {
                throw std::runtime_error( "Server Command packet is outside the receive window" );
            }

            if ( !m_Queued.contains( packetKey ) ) {
                if ( m_Queued.size() >= MaxQueuedPackets ) {
                    throw std::runtime_error( "Server Command receive queue is full" );
                }

                m_Queued.emplace( packetKey, QueuedPacket { .flags = flags, .data = std::move( plaintext ) } );
            }

            return ready;
        }

        ProcessOrdered( flags, plaintext, ready );
        expected.Advance();

        while ( true ) {
            const std::uint64_t nextKey = SequenceKey( expected );
            const auto queued = m_Queued.find( nextKey );

            if ( queued == m_Queued.end() ) {
                break;
            }

            QueuedPacket queuedPacket = std::move( queued->second );
            m_Queued.erase( queued );

            ProcessOrdered( queuedPacket.flags, queuedPacket.data, ready );
            expected.Advance();
        }

        return ready;
    }

    void CommandReceiveWindow::ProcessOrdered( PacketFlags flags,
                                               std::span<const std::byte> data,
                                               std::vector<std::vector<std::byte>>& ready ) {
        if ( data.size() > packet_limits::MaxCommandSize - m_Fragments.size() ) {
            throw std::runtime_error( "Session command is too large" );
        }

        if ( !m_Assembling ) {
            const bool fragmented = HasFlag( flags, PacketFlags::Fragmented );
            const bool compressed = HasFlag( flags, PacketFlags::Compressed );

            if ( !fragmented ) {
                if ( compressed ) {
                    ready.push_back( QuickLz::Decompress( data, packet_limits::MaxCommandSize ) );
                } else {
                    ready.emplace_back( data.begin(), data.end() );
                }

                return;
            }

            m_Assembling = true;
            m_Compressed = compressed;
            m_Fragments.clear();
            m_Fragments.insert( m_Fragments.end(), data.begin(), data.end() );

            return;
        }

        m_Fragments.insert( m_Fragments.end(), data.begin(), data.end() );

        const bool finalFragment = HasFlag( flags, PacketFlags::Fragmented );

        if ( !finalFragment ) {
            return;
        }

        if ( m_Compressed ) {
            ready.push_back( QuickLz::Decompress( m_Fragments, packet_limits::MaxCommandSize ) );
        } else {
            ready.push_back( std::move( m_Fragments ) );
        }

        m_Fragments.clear();
        m_Assembling = false;
        m_Compressed = false;
    }

} // namespace ts::protocol
