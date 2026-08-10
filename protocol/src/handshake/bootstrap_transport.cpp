#include <cstddef>
#include <cstdint>
#include <map>
#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>
#include <protocol/handshake/bootstrap_transport.hpp>
#include <protocol/packet/client_header.hpp>
#include <protocol/packet/codec.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/server_packet.hpp>
#include <span>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    BootstrapTransport::BootstrapTransport( Transport& transport ): m_Transport( transport ) {
        /*
         * Normal TeamSpeak packet counters start at 1.
         *
         * Bootstrap has special starting values:
         *
         * Client Command: 1
         * Client Ack:     0
         * Server Command: 0
         * Server Ack:     0
         */
        m_Sequences.Outgoing( PacketType::Command ) = PacketSequence { .packetId = 1, .generationId = 0 };

        m_Sequences.Outgoing( PacketType::Ack ) = PacketSequence { .packetId = 0, .generationId = 0 };

        m_Sequences.Incoming( PacketType::Command ) = PacketSequence { .packetId = 0, .generationId = 0 };

        m_Sequences.Incoming( PacketType::Ack ) = PacketSequence { .packetId = 0, .generationId = 0 };
    }

    ServerPacket BootstrapTransport::ReceivePacket() {
        return ServerPacket::Parse( m_Transport.Receive() );
    }

    void BootstrapTransport::SendEncrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data ) {
        PacketSequence& sequence = m_Sequences.Outgoing( type );

        ClientPacketHeader header {};

        header.packetId = sequence.packetId;

        header.clientId = 0;

        header.type = type;

        header.flags = flags;

        BinaryWriter metaWriter;

        PacketCodec::WriteClientMeta( metaWriter, header );

        const auto meta = metaWriter.Take();

        const BootstrapEncryptedData encrypted = m_Crypto.Encrypt( meta, data );

        header.mac = encrypted.mac;

        BinaryWriter packetWriter;

        PacketCodec::WriteClientHeader( packetWriter, header );

        packetWriter.WriteBytes( encrypted.data );

        m_Transport.Send( Packet( packetWriter.Take() ) );

        sequence.Advance();
    }

    void BootstrapTransport::SendAck( const ServerPacket& packet ) {
        BinaryWriter writer;

        writer.WriteU16( packet.Header().packetId );

        const auto payload = writer.Take();

        SendEncrypted( PacketType::Ack, PacketFlags::None, payload );
    }

    std::uint16_t BootstrapTransport::SendCommand( std::span<const std::byte> data ) {
        if ( data.size() > MaxClientPacketData ) {
            throw std::runtime_error( "Bootstrap client command is too large" );
        }

        const std::uint16_t packetId = m_Sequences.Outgoing( PacketType::Command ).packetId;

        SendEncrypted( PacketType::Command, PacketFlags::NewProtocol, data );

        return packetId;
    }

    void BootstrapTransport::ReceiveAck( std::uint16_t expectedCommandPacketId ) {
        const ServerPacket packet = ReceivePacket();

        if ( packet.Header().type != PacketType::Ack ) {
            throw std::runtime_error( "Expected bootstrap Ack packet" );
        }

        PacketSequence& sequence = m_Sequences.Incoming( PacketType::Ack );

        if ( packet.Header().packetId != sequence.packetId ) {
            throw std::runtime_error( "Unexpected bootstrap Ack sequence" );
        }

        const auto plaintext = m_Crypto.Decrypt( packet );

        if ( plaintext.size() != 2 ) {
            throw std::runtime_error( "Unexpected bootstrap Ack payload size" );
        }

        BinaryReader reader( plaintext );

        const std::uint16_t acknowledgedPacketId = reader.ReadU16();

        if ( acknowledgedPacketId != expectedCommandPacketId ) {
            throw std::runtime_error( "Unexpected bootstrap Ack packet ID" );
        }

        sequence.Advance();
    }

    std::vector<std::byte> BootstrapTransport::ReceiveCommand() {
        struct Fragment {
            PacketFlags flags;
            std::vector<std::byte> data;
        };

        std::map<std::uint16_t, Fragment> fragments;

        std::size_t totalSize = 0;

        while ( true ) {
            const ServerPacket packet = ReceivePacket();

            if ( packet.Header().type != PacketType::Command ) {
                throw std::runtime_error( "Expected bootstrap Command packet" );
            }

            const std::uint16_t packetId = packet.Header().packetId;

            const auto plaintext = m_Crypto.Decrypt( packet );

            /*
             * Reliable TeamSpeak Command packets must be
             * acknowledged individually.
             *
             * Retransmissions are acknowledged again because
             * our previous ACK may have been lost.
             */
            SendAck( packet );

            if ( fragments.contains( packetId ) ) {
                continue;
            }

            if ( fragments.size() >= MaxFragments ) {
                throw std::runtime_error( "Too many bootstrap command fragments" );
            }

            if ( plaintext.size() > MaxCommandSize - totalSize ) {
                throw std::runtime_error( "Bootstrap command is too large" );
            }

            totalSize += plaintext.size();

            fragments.emplace( packetId, Fragment { .flags = packet.Header().flags, .data = plaintext } );

            /*
             * The first bootstrap server Command starts at
             * packet ID 0.
             */
            const auto first = fragments.find( 0 );

            if ( first == fragments.end() ) {
                continue;
            }

            if ( HasFlag( first->second.flags, PacketFlags::Compressed ) ) {
                throw std::runtime_error( "Compressed bootstrap commands are not supported" );
            }

            const bool fragmented = HasFlag( first->second.flags, PacketFlags::Fragmented );

            if ( !fragmented ) {
                PacketSequence& sequence = m_Sequences.Incoming( PacketType::Command );

                sequence.packetId = packetId;

                sequence.Advance();

                return first->second.data;
            }

            /*
             * TeamSpeak marks both the first and final
             * fragment with Fragmented. Middle fragments do
             * not carry the flag.
             */
            for ( std::uint32_t lastId = 1; lastId < MaxFragments; ++lastId ) {
                const auto current = fragments.find( static_cast<std::uint16_t>( lastId ) );

                if ( current == fragments.end() ) {
                    break;
                }

                if ( !HasFlag( current->second.flags, PacketFlags::Fragmented ) ) {
                    continue;
                }

                std::vector<std::byte> result;

                result.reserve( totalSize );

                for ( std::uint32_t id = 0; id <= lastId; ++id ) {
                    const auto fragment = fragments.find( static_cast<std::uint16_t>( id ) );

                    if ( fragment == fragments.end() ) {
                        throw std::runtime_error( "Missing bootstrap command fragment" );
                    }

                    result.insert( result.end(), fragment->second.data.begin(), fragment->second.data.end() );
                }

                PacketSequence& sequence = m_Sequences.Incoming( PacketType::Command );

                sequence.packetId = static_cast<std::uint16_t>( lastId );

                sequence.Advance();

                return result;
            }
        }
    }

    PacketSequenceState BootstrapTransport::SequenceState() const {
        return m_Sequences;
    }

} // namespace ts::protocol
