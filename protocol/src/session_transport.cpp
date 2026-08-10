#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>
#include <protocol/packet/client_header.hpp>
#include <protocol/packet/codec.hpp>
#include <protocol/packet/command_fragmenter.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/sequence.hpp>
#include <protocol/packet/server_packet.hpp>
#include <protocol/session_transport.hpp>
#include <protocol/voice/voice_codec.hpp>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    SessionTransport::SessionTransport( Transport& transport, SessionCrypto& crypto, PacketSequenceState sequences ):
        m_Transport( transport ), m_Crypto( crypto ), m_Sequences( std::move( sequences ) ) {
    }

    ServerPacket SessionTransport::ReceivePacket() {
        return ServerPacket::Parse( m_Transport.Receive() );
    }

    Packet SessionTransport::CreateEncryptedPacket( PacketType type,
                                                    PacketFlags flags,
                                                    std::span<const std::byte> data,
                                                    const PacketSequence& sequence ) const {
        ClientPacketHeader header {};

        header.packetId = sequence.packetId;
        header.clientId = m_ClientId;
        header.type = type;
        header.flags = flags;

        BinaryWriter metaWriter;
        PacketCodec::WriteClientMeta( metaWriter, header );

        const auto meta = metaWriter.Take();

        const SessionEncryptedData encrypted =
            m_Crypto.EncryptClient( type, sequence.packetId, sequence.generationId, meta, data );

        header.mac = encrypted.mac;

        BinaryWriter packetWriter;
        PacketCodec::WriteClientHeader( packetWriter, header );
        packetWriter.WriteBytes( encrypted.data );

        return Packet( packetWriter.Take() );
    }

    Packet SessionTransport::CreateUnencryptedPacket( PacketType type,
                                                      PacketFlags flags,
                                                      std::span<const std::byte> data,
                                                      const PacketSequence& sequence ) const {
        ClientPacketHeader header {};

        header.mac = m_Crypto.SharedMac();
        header.packetId = sequence.packetId;
        header.clientId = m_ClientId;
        header.type = type;
        header.flags = flags | PacketFlags::Unencrypted;

        BinaryWriter writer;
        PacketCodec::WriteClientHeader( writer, header );
        writer.WriteBytes( data );

        return Packet( writer.Take() );
    }

    void SessionTransport::SendEncrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data ) {
        if ( data.size() > packet_limits::MaxClientPayload ) {
            throw std::runtime_error( "TeamSpeak client packet payload is too large" );
        }

        PacketSequence& sequence = m_Sequences.Outgoing( type );

        const Packet packet = CreateEncryptedPacket( type, flags, data, sequence );

        m_Transport.Send( packet );
        m_Statistics.RecordSent( type, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();
    }

    void SessionTransport::SendUnencrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data ) {
        if ( data.size() > packet_limits::MaxClientPayload ) {
            throw std::runtime_error( "TeamSpeak client packet payload is too large" );
        }

        PacketSequence& sequence = m_Sequences.Outgoing( type );

        const Packet packet = CreateUnencryptedPacket( type, flags, data, sequence );

        m_Transport.Send( packet );
        m_Statistics.RecordSent( type, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();
    }

    std::uint16_t SessionTransport::SendCommand( std::span<const std::byte> data ) {
        const std::vector<CommandFragment> fragments = CommandFragmenter::Plan( data.size() );

        std::uint16_t lastPacketId = 0;

        for ( const CommandFragment& fragment : fragments ) {
            lastPacketId = SendCommandPacket( data.subspan( fragment.offset, fragment.size ), fragment.flags );
        }

        return lastPacketId;
    }

    std::uint16_t SessionTransport::SendCommandPacket( std::span<const std::byte> data, PacketFlags flags ) {
        PacketSequence& sequence = m_Sequences.Outgoing( PacketType::Command );

        const std::uint16_t packetId = sequence.packetId;
        const std::uint32_t generationId = sequence.generationId;
        const Packet packet = CreateEncryptedPacket( PacketType::Command, flags, data, sequence );

        m_ReliableCommands.Add( packetId, generationId, packet, ReliableCommandQueue::Clock::now() );

        if ( !m_FirstCommandPacketId ) {
            m_FirstCommandPacketId = packetId;
        }

        m_Transport.Send( packet );
        m_Statistics.RecordSent( PacketType::Command, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();

        return packetId;
    }

    void SessionTransport::SendVoice( VoiceCodec codec, std::span<const std::byte> data, bool encrypted, bool talkStart ) {
        PacketSequence& sequence = m_Sequences.Outgoing( PacketType::Voice );
        const std::vector<std::byte> payload = VoiceWireCodec::EncodeClient( sequence.packetId, codec, data );

        const PacketFlags flags = talkStart ? PacketFlags::Compressed : PacketFlags::None;
        const Packet packet = encrypted ? CreateEncryptedPacket( PacketType::Voice, flags, payload, sequence )
                                        : CreateUnencryptedPacket( PacketType::Voice, flags, payload, sequence );

        m_Transport.Send( packet );
        m_Statistics.RecordSent( PacketType::Voice, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();
    }

    void SessionTransport::SendVoiceWhisper( VoiceCodec codec,
                                             std::span<const std::uint64_t> channelIds,
                                             std::span<const std::uint16_t> clientIds,
                                             std::span<const std::byte> data,
                                             bool encrypted,
                                             bool talkStart ) {
        PacketSequence& sequence = m_Sequences.Outgoing( PacketType::VoiceWhisper );
        const std::vector<std::byte> payload =
            VoiceWireCodec::EncodeClientWhisper( sequence.packetId, codec, channelIds, clientIds, data );
        const PacketFlags flags = talkStart ? PacketFlags::Compressed : PacketFlags::None;
        const Packet packet = encrypted ? CreateEncryptedPacket( PacketType::VoiceWhisper, flags, payload, sequence )
                                        : CreateUnencryptedPacket( PacketType::VoiceWhisper, flags, payload, sequence );

        m_Transport.Send( packet );
        m_Statistics.RecordSent( PacketType::VoiceWhisper, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();
    }
    void SessionTransport::SendGroupWhisper( VoiceCodec codec,
                                             GroupWhisperType type,
                                             GroupWhisperTarget target,
                                             std::uint64_t targetId,
                                             std::span<const std::byte> data,
                                             bool encrypted,
                                             bool talkStart ) {
        PacketSequence& sequence = m_Sequences.Outgoing( PacketType::VoiceWhisper );
        const std::vector<std::byte> payload =
            VoiceWireCodec::EncodeClientGroupWhisper( sequence.packetId, codec, type, target, targetId, data );
        PacketFlags flags = PacketFlags::NewProtocol;
        if ( talkStart ) {
            flags = flags | PacketFlags::Compressed;
        }
        const Packet packet = encrypted ? CreateEncryptedPacket( PacketType::VoiceWhisper, flags, payload, sequence )
                                        : CreateUnencryptedPacket( PacketType::VoiceWhisper, flags, payload, sequence );

        m_Transport.Send( packet );
        m_Statistics.RecordSent( PacketType::VoiceWhisper, packet.Size() + packet_limits::Ipv4UdpOverhead );
        sequence.Advance();
    }
    void SessionTransport::ConfirmCommand( std::uint16_t packetId ) {
        (void)m_ReliableCommands.Acknowledge( packetId );
    }

    bool SessionTransport::IsCommandPending( std::uint16_t packetId ) const {
        return m_ReliableCommands.Contains( packetId );
    }

    bool SessionTransport::HasPendingReliableCommands() const {
        return m_ReliableCommands.Size() != 0;
    }

    void SessionTransport::SendPing( TimePoint now ) {
        PacketSequence& sequence = m_Sequences.Outgoing( PacketType::Ping );
        const std::uint16_t packetId = sequence.packetId;

        SendUnencrypted( PacketType::Ping, PacketFlags::None, {} );

        m_PendingPing = PendingPing { .packetId = packetId, .sentAt = now };
    }

    void SessionTransport::SendAck( const ServerPacket& packet, PacketType ackType ) {
        BinaryWriter writer;
        writer.WriteU16( packet.Header().packetId );

        const auto payload = writer.Take();

        SendEncrypted( ackType, PacketFlags::None, payload );
    }

    void SessionTransport::SendPong( std::uint16_t pingPacketId ) {
        BinaryWriter writer;
        writer.WriteU16( pingPacketId );

        const auto payload = writer.Take();

        SendUnencrypted( PacketType::Pong, PacketFlags::None, payload );
    }

    void SessionTransport::HandleAck( const ServerPacket& packet ) {
        PacketSequence& expected = m_Sequences.Incoming( PacketType::Ack );

        const std::uint32_t generationId = ResolveGeneration( expected, packet.Header().packetId );
        const std::uint64_t packetKey = SequenceKey( packet.Header().packetId, generationId );
        const std::uint64_t expectedKey = SequenceKey( expected );

        /*
         * TeamSpeak has a special first Ack after clientinit, deliberately not
         * decrypted here (see comment below at DecryptServer). Its own header
         * carries the server's ack-sequence position, not the packet ID it is
         * acknowledging, so it cannot be matched against the reliable queue
         * the normal way. Retire the session's first command here instead of
         * waiting solely for initserver to do it via ConfirmCommand: if
         * initserver is delayed past the retransmission timer, the queue
         * would otherwise resend clientinit even though it was already
         * accepted, and a server that has moved past login silently drops
         * that duplicate rather than answering it again.
         */
        if ( generationId == 0 && packet.Header().packetId == 1 ) {
            if ( m_FirstCommandPacketId ) {
                (void)m_ReliableCommands.Acknowledge( *m_FirstCommandPacketId );
            }

            if ( packetKey >= expectedKey ) {
                expected.packetId = packet.Header().packetId;
                expected.generationId = generationId;
                expected.Advance();
            }

            return;
        }

        std::vector<std::byte> plaintext;

        if ( HasFlag( packet.Header().flags, PacketFlags::Unencrypted ) ) {
            /*
             * Some TeamSpeak servers send Ack packets unencrypted.
             *
             * Unencrypted session packets still carry SharedMac, so verify it
             * before trusting the acknowledged packet id.
             */
            if ( !std::equal( packet.Header().mac.begin(),
                              packet.Header().mac.end(),
                              m_Crypto.SharedMac().begin(),
                              m_Crypto.SharedMac().end() ) ) {
                throw std::runtime_error( "Invalid server Ack MAC" );
            }

            plaintext.assign( packet.Data().begin(), packet.Data().end() );
        } else {
            plaintext = m_Crypto.DecryptServer( packet, generationId );
        }

        if ( plaintext.size() != 2 ) {
            throw std::runtime_error( "Unexpected server Ack payload size" );
        }

        BinaryReader reader( plaintext );
        const std::uint16_t acknowledgedPacketId = reader.ReadU16();

        const auto acknowledgement = m_ReliableCommands.Acknowledge( acknowledgedPacketId );

        if ( acknowledgement && !acknowledgement->retransmitted ) {
            m_Statistics.RecordRtt( Clock::now() - acknowledgement->firstSent );
        }

        if ( packetKey >= expectedKey ) {
            expected.packetId = packet.Header().packetId;
            expected.generationId = generationId;
            expected.Advance();
        }
    }

    void SessionTransport::HandleAckLow( const ServerPacket& packet ) {
        PacketSequence& expected = m_Sequences.Incoming( PacketType::AckLow );

        const std::uint32_t generationId = ResolveGeneration( expected, packet.Header().packetId );
        const std::uint64_t packetKey = SequenceKey( packet.Header().packetId, generationId );
        const std::uint64_t expectedKey = SequenceKey( expected );

        std::vector<std::byte> plaintext;

        if ( HasFlag( packet.Header().flags, PacketFlags::Unencrypted ) ) {
            /*
             * Unencrypted session packets still carry SharedMac, so verify it
             * before trusting this AckLow at all (see the matching comment
             * on HandleAck).
             */
            if ( !std::equal( packet.Header().mac.begin(),
                              packet.Header().mac.end(),
                              m_Crypto.SharedMac().begin(),
                              m_Crypto.SharedMac().end() ) ) {
                throw std::runtime_error( "Invalid server AckLow MAC" );
            }

            plaintext.assign( packet.Data().begin(), packet.Data().end() );
        } else {
            plaintext = m_Crypto.DecryptServer( packet, generationId );
        }

        if ( plaintext.size() != 2 ) {
            throw std::runtime_error( "Unexpected server AckLow payload size" );
        }

        /*
         * ts-cli never sends CommandLow (see the comment on m_ReliableCommands),
         * so there is nothing to acknowledge yet -- this only validates the
         * packet and advances the AckLow sequence, unlike HandleAck there is
         * no reliable queue to match the acknowledged packet id against.
         */

        if ( packetKey >= expectedKey ) {
            expected.packetId = packet.Header().packetId;
            expected.generationId = generationId;
            expected.Advance();
        }
    }

    void SessionTransport::HandlePing( const ServerPacket& packet ) {
        if ( !HasFlag( packet.Header().flags, PacketFlags::Unencrypted ) ) {
            throw std::runtime_error( "Server Ping packet is encrypted" );
        }

        if ( !std::equal( packet.Header().mac.begin(),
                          packet.Header().mac.end(),
                          m_Crypto.SharedMac().begin(),
                          m_Crypto.SharedMac().end() ) ) {
            throw std::runtime_error( "Invalid server Ping MAC" );
        }

        SendPong( packet.Header().packetId );
    }

    void SessionTransport::HandlePong( const ServerPacket& packet ) {
        if ( !HasFlag( packet.Header().flags, PacketFlags::Unencrypted ) ) {
            throw std::runtime_error( "Server Pong packet is encrypted" );
        }

        if ( !std::equal( packet.Header().mac.begin(),
                          packet.Header().mac.end(),
                          m_Crypto.SharedMac().begin(),
                          m_Crypto.SharedMac().end() ) ) {
            throw std::runtime_error( "Invalid server Pong MAC" );
        }

        if ( packet.Data().size() != 2 ) {
            throw std::runtime_error( "Unexpected server Pong payload size" );
        }

        BinaryReader reader( packet.Data() );
        const std::uint16_t pingPacketId = reader.ReadU16();

        if ( m_PendingPing && m_PendingPing->packetId == pingPacketId ) {
            m_Statistics.RecordRtt( Clock::now() - m_PendingPing->sentAt );
            m_PendingPing.reset();
        }
    }

    void SessionTransport::ProcessIncomingCommand( PacketType dataType,
                                                   PacketType ackType,
                                                   CommandReceiveWindow& window,
                                                   const ServerPacket& packet ) {
        PacketSequence& expected = m_Sequences.Incoming( dataType );
        const std::uint32_t generationId = ResolveGeneration( expected, packet.Header().packetId );

        auto plaintext = m_Crypto.DecryptServer( packet, generationId );

        SendAck( packet, ackType );

        std::vector<std::vector<std::byte>> readyCommands =
            window.Feed( expected, packet.Header().packetId, generationId, packet.Header().flags, std::move( plaintext ) );

        for ( std::vector<std::byte>& command : readyCommands ) {
            m_ReadyCommands.push_back( std::move( command ) );
        }
    }

    void SessionTransport::HandleCommand( const ServerPacket& packet ) {
        ProcessIncomingCommand( PacketType::Command, PacketType::Ack, m_CommandWindow, packet );
    }

    void SessionTransport::HandleCommandLow( const ServerPacket& packet ) {
        ProcessIncomingCommand( PacketType::CommandLow, PacketType::AckLow, m_CommandLowWindow, packet );
    }

    void SessionTransport::HandleVoice( const ServerPacket& packet ) {
        const PacketType type = packet.Header().type;
        PacketSequence& expected = m_Sequences.Incoming( type );
        const std::uint32_t generationId = ResolveGeneration( expected, packet.Header().packetId );

        std::vector<std::byte> plaintext;

        if ( HasFlag( packet.Header().flags, PacketFlags::Unencrypted ) ) {
            if ( !std::equal( packet.Header().mac.begin(),
                              packet.Header().mac.end(),
                              m_Crypto.SharedMac().begin(),
                              m_Crypto.SharedMac().end() ) ) {
                throw std::runtime_error( "Invalid unencrypted server Voice MAC" );
            }

            plaintext.assign( packet.Data().begin(), packet.Data().end() );
        } else {
            plaintext = m_Crypto.DecryptServer( packet, generationId );
        }

        VoiceFrame frame = VoiceWireCodec::DecodeServer( plaintext, packet.Header().flags, type == PacketType::VoiceWhisper );
        m_ReadyVoice.push_back( std::move( frame ) );

        const std::uint64_t packetKey = SequenceKey( packet.Header().packetId, generationId );
        const std::uint64_t expectedKey = SequenceKey( expected );

        if ( packetKey >= expectedKey ) {
            expected.packetId = packet.Header().packetId;
            expected.generationId = generationId;
            expected.Advance();
        }
    }

    void SessionTransport::ProcessPacket() {
        const ServerPacket packet = ReceivePacket();

        switch ( packet.Header().type ) {
            case PacketType::Ack:
                HandleAck( packet );
                break;

            case PacketType::Ping:
                HandlePing( packet );
                break;

            case PacketType::Pong:
                HandlePong( packet );
                break;

            case PacketType::Command:
                HandleCommand( packet );
                break;

            case PacketType::Voice:
            case PacketType::VoiceWhisper:
                HandleVoice( packet );
                break;

            case PacketType::CommandLow:
                HandleCommandLow( packet );
                break;

            case PacketType::AckLow:
                HandleAckLow( packet );
                break;

            case PacketType::Init1:
                throw std::runtime_error( "Unexpected Init1 packet in established session" );
        }

        const TimePoint now = Clock::now();

        m_Statistics.RecordReceived( packet.Header().type,
                                     packet.Header().packetId,
                                     packet_limits::ServerHeaderSize + packet.Data().size() + packet_limits::Ipv4UdpOverhead,
                                     now );

        if ( m_Connected ) {
            /*
             * Any authenticated server packet proves the connection is alive.
             * Keep the pending Ping until HandlePong() gets a chance to match it;
             * ProcessTimers() will retire it if unrelated server traffic arrived
             * after the probe was sent.
             */
            m_LastReceiveAt = now;
        }
    }

    void SessionTransport::ProcessTimers() {
        const TimePoint now = Clock::now();
        const auto packets = m_ReliableCommands.CollectDue( now );

        for ( const Packet& packet : packets ) {
            m_Transport.Send( packet );
            m_Statistics.RecordSent( PacketType::Command, packet.Size() + packet_limits::Ipv4UdpOverhead );
        }

        if ( !m_Connected || HasPendingReliableCommands() ) {
            return;
        }

        if ( m_PendingPing ) {
            /*
             * A packet received after the probe already proves liveness. Do not
             * keep generating Ping traffic just because that packet was not the
             * matching Pong.
             */
            if ( m_LastReceiveAt > m_PendingPing->sentAt ) {
                m_PendingPing.reset();
                return;
            }

            /*
             * During genuine silence, retry at the normal keepalive cadence.
             * Ping is intentionally not reliable and never enters the Command
             * retransmission queue.
             */
            if ( now - m_PendingPing->sentAt >= PingInterval ) {
                m_PendingPing.reset();
                SendPing( now );
            }

            return;
        }

        if ( now - m_LastReceiveAt >= PingInterval ) {
            SendPing( now );
        }
    }

    std::optional<ReliableCommandQueue::TimePoint> SessionTransport::NextDeadline() const {
        return m_ReliableCommands.NextDeadline();
    }

    bool SessionTransport::HasReadyCommand() const {
        return !m_ReadyCommands.empty();
    }

    std::vector<std::byte> SessionTransport::TakeReadyCommand() {
        if ( m_ReadyCommands.empty() ) {
            throw std::runtime_error( "No session command is ready" );
        }

        std::vector<std::byte> result = std::move( m_ReadyCommands.front() );
        m_ReadyCommands.pop_front();

        return result;
    }

    bool SessionTransport::HasReadyVoice() const {
        return !m_ReadyVoice.empty();
    }

    VoiceFrame SessionTransport::TakeReadyVoice() {
        if ( m_ReadyVoice.empty() ) {
            throw std::runtime_error( "No voice frame is ready" );
        }

        VoiceFrame frame = std::move( m_ReadyVoice.front() );
        m_ReadyVoice.pop_front();
        return frame;
    }

    std::vector<std::byte> SessionTransport::ReceiveCommand() {
        using namespace std::chrono_literals;

        while ( !HasReadyCommand() ) {
            ProcessTimers();

            if ( !m_Transport.WaitReadable( WaitTimeout( 1s ) ) ) {
                continue;
            }

            ProcessPacket();
        }

        return TakeReadyCommand();
    }

    void SessionTransport::SetClientId( std::uint16_t clientId ) {
        if ( clientId == 0 ) {
            throw std::runtime_error( "Invalid TeamSpeak client ID" );
        }

        m_ClientId = clientId;
    }

    void SessionTransport::SetConnected() {
        const TimePoint now = Clock::now();

        m_Connected = true;
        m_Statistics.Reset( now );
        m_LastReceiveAt = now;
        m_PendingPing.reset();
    }

    std::uint16_t SessionTransport::ClientId() const {
        return m_ClientId;
    }

    ConnectionStatistics::Snapshot SessionTransport::Statistics() const {
        return m_Statistics.GetSnapshot();
    }

    std::chrono::milliseconds SessionTransport::WaitTimeout( std::chrono::milliseconds maximum ) const {
        const auto deadline = NextDeadline();

        if ( !deadline ) {
            return maximum;
        }

        const auto now = ReliableCommandQueue::Clock::now();

        if ( *deadline <= now ) {
            return std::chrono::milliseconds { 0 };
        }

        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( *deadline - now );

        return std::min( maximum, remaining );
    }

} // namespace ts::protocol
