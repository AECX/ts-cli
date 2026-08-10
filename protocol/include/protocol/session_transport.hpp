#ifndef TS_PROTOCOL_SESSION_TRANSPORT_HPP
#define TS_PROTOCOL_SESSION_TRANSPORT_HPP
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <protocol/connection_statistics.hpp>
#include <protocol/crypto/session_crypto.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/sequence_state.hpp>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <protocol/transport.hpp>
#include <protocol/voice/voice.hpp>
#include <span>
#include <vector>
namespace ts::protocol {

    class ServerPacket;

    class SessionTransport {
      public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        SessionTransport( Transport& transport, SessionCrypto& crypto, PacketSequenceState sequences );
        std::uint16_t SendCommand( std::span<const std::byte> data );
        void SendVoice( VoiceCodec codec, std::span<const std::byte> data, bool encrypted, bool talkStart );
        void SendVoiceWhisper( VoiceCodec codec,
                               std::span<const std::uint64_t> channelIds,
                               std::span<const std::uint16_t> clientIds,
                               std::span<const std::byte> data,
                               bool encrypted,
                               bool talkStart );
        void SendGroupWhisper( VoiceCodec codec,
                               GroupWhisperType type,
                               GroupWhisperTarget target,
                               std::uint64_t targetId,
                               std::span<const std::byte> data,
                               bool encrypted,
                               bool talkStart );
        void ConfirmCommand( std::uint16_t packetId );

        [[nodiscard]] bool IsCommandPending( std::uint16_t packetId ) const;
        [[nodiscard]] bool HasPendingReliableCommands() const;

        void ProcessPacket();
        void ProcessTimers();
        [[nodiscard]] std::optional<ReliableCommandQueue::TimePoint> NextDeadline() const;

        [[nodiscard]] bool HasReadyCommand() const;
        [[nodiscard]] std::vector<std::byte> TakeReadyCommand();
        [[nodiscard]] bool HasReadyVoice() const;
        [[nodiscard]] VoiceFrame TakeReadyVoice();

        [[nodiscard]] std::vector<std::byte> ReceiveCommand();

        void SetClientId( std::uint16_t clientId );
        void SetConnected();
        [[nodiscard]] std::uint16_t ClientId() const;
        [[nodiscard]] ConnectionStatistics::Snapshot Statistics() const;

      private:
        struct QueuedCommandPacket {
            PacketFlags flags = PacketFlags::None;
            std::vector<std::byte> data;
        };

        struct PendingPing {
            std::uint16_t packetId = 0;
            TimePoint sentAt;
        };

        static constexpr std::size_t MaxQueuedCommandPackets = 64;
        static constexpr std::chrono::seconds PingInterval { 1 };

        [[nodiscard]] ServerPacket ReceivePacket();

        [[nodiscard]] Packet CreateEncryptedPacket( PacketType type,
                                                    PacketFlags flags,
                                                    std::span<const std::byte> data,
                                                    const PacketSequence& sequence ) const;
        [[nodiscard]] Packet CreateUnencryptedPacket( PacketType type,
                                                      PacketFlags flags,
                                                      std::span<const std::byte> data,
                                                      const PacketSequence& sequence ) const;
        void SendEncrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data );
        void SendUnencrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data );
        std::uint16_t SendCommandPacket( std::span<const std::byte> data, PacketFlags flags );
        void SendPing( TimePoint now );

        void SendAck( const ServerPacket& packet );
        void SendPong( std::uint16_t pingPacketId );
        void HandleAck( const ServerPacket& packet );
        void HandlePing( const ServerPacket& packet );
        void HandlePong( const ServerPacket& packet );
        void HandleCommand( const ServerPacket& packet );
        void HandleVoice( const ServerPacket& packet );

        void ProcessOrderedCommand( PacketFlags flags, std::span<const std::byte> data );

        [[nodiscard]] std::chrono::milliseconds WaitTimeout( std::chrono::milliseconds maximum ) const;
        [[nodiscard]] static std::uint32_t ResolveGeneration( const PacketSequence& expected, std::uint16_t packetId );

        [[nodiscard]] static std::uint64_t SequenceKey( std::uint16_t packetId, std::uint32_t generationId );

        [[nodiscard]] static std::uint64_t SequenceKey( const PacketSequence& sequence );

        Transport& m_Transport;
        SessionCrypto& m_Crypto;
        PacketSequenceState m_Sequences;
        ReliableCommandQueue m_ReliableCommands;
        ConnectionStatistics m_Statistics;

        std::uint16_t m_ClientId = 0;
        bool m_Connected = false;
        TimePoint m_LastReceiveAt {};
        std::optional<PendingPing> m_PendingPing;
        std::optional<std::uint16_t> m_FirstCommandPacketId;

        std::map<std::uint64_t, QueuedCommandPacket> m_QueuedCommands;
        std::deque<std::vector<std::byte>> m_ReadyCommands;
        std::deque<VoiceFrame> m_ReadyVoice;
        bool m_AssemblingCommand = false;
        bool m_CommandCompressed = false;
        std::vector<std::byte> m_CommandFragments;
    };

} // namespace ts::protocol
#endif // TS_PROTOCOL_SESSION_TRANSPORT_HPP
