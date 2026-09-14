#ifndef TS_PROTOCOL_SESSION_TRANSPORT_HPP
#define TS_PROTOCOL_SESSION_TRANSPORT_HPP
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <protocol/connection_statistics.hpp>
#include <protocol/crypto/session_crypto.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/sequence_state.hpp>
#include <protocol/reliability/command_receive_window.hpp>
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

        /*
         * True once nothing at all has been received for ReceiveTimeout.
         *
         * The server is pinged every second and answers every ping, so silence
         * this long means the connection is gone rather than merely idle. Only
         * meaningful after SetConnected -- during the handshake the caller is
         * driving its own blocking exchanges.
         *
         * now is explicit so the boundary can be tested without waiting.
         */
        [[nodiscard]] bool TimedOut( TimePoint now ) const;

        /*
         * The timeout policy itself, separated from the live transport so it
         * can be tested without a socket.
         */
        [[nodiscard]] static bool IsTimedOut( TimePoint lastReceiveAt, TimePoint now );

        static constexpr std::chrono::seconds ReceiveTimeout { 30 };

      private:
        struct PendingPing {
            std::uint16_t packetId = 0;
            TimePoint sentAt;
        };

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

        void SendAck( const ServerPacket& packet, PacketType ackType );
        void SendPong( std::uint16_t pingPacketId );
        void HandleAck( const ServerPacket& packet );
        void HandleAckLow( const ServerPacket& packet );
        void HandlePing( const ServerPacket& packet );
        void HandlePong( const ServerPacket& packet );
        void HandleCommand( const ServerPacket& packet );
        void HandleCommandLow( const ServerPacket& packet );
        void HandleVoice( const ServerPacket& packet );

        void ProcessIncomingCommand( PacketType dataType,
                                     PacketType ackType,
                                     CommandReceiveWindow& window,
                                     const ServerPacket& packet );

        [[nodiscard]] std::chrono::milliseconds WaitTimeout( std::chrono::milliseconds maximum ) const;

        Transport& m_Transport;
        SessionCrypto& m_Crypto;
        PacketSequenceState m_Sequences;
        // Outgoing reliable retransmission tracking for the Command stream
        // only -- ts-cli never originates CommandLow traffic today. If that
        // changes, it needs its own ReliableCommandQueue instance: packetIds
        // are independent per stream (each starts at 1) and would collide if
        // shared with this queue.
        ReliableCommandQueue m_ReliableCommands;
        ConnectionStatistics m_Statistics;

        std::uint16_t m_ClientId = 0;
        bool m_Connected = false;
        TimePoint m_LastReceiveAt {};
        std::optional<PendingPing> m_PendingPing;
        std::optional<std::uint16_t> m_FirstCommandPacketId;

        CommandReceiveWindow m_CommandWindow;
        CommandReceiveWindow m_CommandLowWindow;
        std::deque<std::vector<std::byte>> m_ReadyCommands;
        std::deque<VoiceFrame> m_ReadyVoice;
    };

} // namespace ts::protocol
#endif // TS_PROTOCOL_SESSION_TRANSPORT_HPP
