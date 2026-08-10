#ifndef TS_PROTOCOL_SESSION_SESSION_HPP
#define TS_PROTOCOL_SESSION_SESSION_HPP

#include "bootstrap.hpp"
#include "event.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <protocol/client_profile.hpp>
#include <protocol/command/command.hpp>
#include <protocol/crypto/session_crypto.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <protocol/session_transport.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <protocol/transport.hpp>
#include <span>
#include <string>
#include <string_view>

namespace ts::protocol {

    class Session {
      public:
        Session( Transport& transport, SessionBootstrap bootstrap );
        Session( const Session& ) = delete;

        Session& operator=( const Session& ) = delete;

        Session( Session&& ) = delete;

        Session& operator=( Session&& ) = delete;

        void Login( const ClientProfile& profile, std::uint64_t keyOffset );
        void Disconnect( std::string_view reason );

        void SendTextMessage( TextMessageTarget target, std::string_view text );
        void MoveToChannel( std::uint64_t channelId );
        void ChangeNickname( std::string_view nickname );
        void SetAudioState( bool inputHardware, bool outputHardware, bool inputMuted );
        void SendVoice( std::span<const std::byte> data, bool talkStart );

        void ProcessPacket();
        void ProcessTimers();

        [[nodiscard]] std::optional<ReliableCommandQueue::TimePoint> NextDeadline() const;
        [[nodiscard]] bool DisconnectPending() const;

        [[nodiscard]] bool HasEvent() const;
        [[nodiscard]] SessionEvent TakeEvent();

        [[nodiscard]] std::uint16_t ClientId() const;
        [[nodiscard]] std::uint64_t CurrentChannelId() const;
        [[nodiscard]] std::string_view CurrentNickname() const;
        [[nodiscard]] std::string_view ServerName() const;
        [[nodiscard]] const ChannelStore& Channels() const;
        [[nodiscard]] const ClientStore& Clients() const;

      private:
        void ReceiveInitialState();
        void SubscribeAllChannels();
        void SubscribeChannel( std::uint64_t channelId );
        void ProcessReadyCommands();
        void ProcessCommand( const Command& command );
        void SendConnectionInfo();
        void EmitPresence( ClientPresenceKind kind,
                           std::uint16_t clientId,
                           std::uint64_t channelId,
                           std::string_view clientName );

        [[nodiscard]] std::optional<TextMessageTarget> ReplyTargetFor( const TextMessageEntry& message ) const;
        [[nodiscard]] std::string ChannelNameFor( const TextMessageEntry& message ) const;
        [[nodiscard]] std::string PrivatePeerNameFor( const TextMessageEntry& message ) const;
        [[nodiscard]] VoiceCodec CurrentVoiceCodec() const;
        [[nodiscard]] bool VoiceEncrypted() const;

        SessionCrypto m_SessionCrypto;
        SessionTransport m_SessionTransport;

        std::uint16_t m_ClientId = 0;
        std::uint64_t m_CurrentChannelId = 0;
        std::string m_CurrentNickname;
        std::string m_ServerName;
        std::uint8_t m_CodecEncryptionMode = 0;

        ChannelStore m_Channels;
        ClientStore m_Clients;

        bool m_InitialStateComplete = false;
        bool m_ConnectionInfoRequested = false;
        std::optional<ReliableCommandQueue::TimePoint> m_LastConnectionInfoSentAt;
        std::optional<std::uint16_t> m_DisconnectPacketId;
        std::deque<SessionEvent> m_Events;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_SESSION_HPP
