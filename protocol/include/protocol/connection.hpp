#ifndef TS_PROTOCOL_CONNECTION_HPP
#define TS_PROTOCOL_CONNECTION_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <net/address.hpp>
#include <net/udp_socket.hpp>
#include <optional>
#include <protocol/client_profile.hpp>
#include <protocol/connection_statistics.hpp>
#include <protocol/handshake/handshake.hpp>
#include <protocol/identity.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/session/audio_state.hpp>
#include <protocol/session/disconnect.hpp>
#include <protocol/session/event.hpp>
#include <protocol/transport.hpp>
#include <protocol/voice/voice.hpp>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

namespace ts::protocol {

    class ChannelStore;
    class ClientStore;
    class Session;

    using SessionEventHandler = std::function<void( SessionEvent )>;

    /*
     * Invoked once per loop iteration on the network owner thread, before
     * packets are pumped. This is where a caller drains its own action queue:
     * it is the only safe place to touch the Connection from another
     * component's work, because it runs on the thread that owns session state.
     */
    using ConnectionCycleHandler = std::function<void()>;

    /*
     * Everything needed to establish a session. Move-only, because Identity
     * owns a private key that is deliberately not copyable.
     */
    struct ConnectionOptions {
        /* "host", "host:port", or a bare address; see net::ParseEndpoint. */
        std::string endpoint;

        ClientProfile profile;

        Identity identity;

        /*
         * The identity's hashcash counter. It belongs to the identity rather
         * than to the connection, and is what the server checks to decide
         * whether the identity's security level is high enough to join.
         */
        std::uint64_t identityKeyOffset = 0;
    };

    enum class ConnectionState {
        /* Constructed; the endpoint has been resolved but Connect has not run. */
        Created,

        /* Connect is in progress: handshake and login. */
        Connecting,

        /* Logged in, initial state received, safe to send and read state. */
        Established,

        /* The session is over. A Connection is not re-usable after this. */
        Closed
    };

    class Connection {
      public:
        explicit Connection( ConnectionOptions options );

        ~Connection();

        Connection( const Connection& ) = delete;
        Connection& operator=( const Connection& ) = delete;

        Connection( Connection&& ) = delete;
        Connection& operator=( Connection&& ) = delete;

        /*
         * Run the handshake and log in. Blocks until the server has sent the
         * initial channel/client state. Throws on failure, leaving the
         * Connection Closed.
         */
        void Connect();

        /*
         * Leave the server politely: send the disconnect command and pump the
         * session until the server acknowledges it, or a short timeout passes.
         * The Connection is Closed afterwards either way.
         */
        void Disconnect( std::string_view reason = "Client disconnected" );

        /*
         * Advance the session once: run timers, then wait up to timeout for a
         * packet and process it. Returns whether a packet was processed.
         *
         * This is the building block for callers that own their own loop.
         * Drain the resulting events with HasEvent/TakeEvent. Everything this
         * class does must happen on one thread; see Wake.
         */
        bool Poll( std::chrono::milliseconds timeout );

        [[nodiscard]] bool HasEvent() const;
        [[nodiscard]] SessionEvent TakeEvent();

        /*
         * Long-lived established-session loop, built on Poll/TakeEvent.
         * Exactly this thread owns all mutable session state while Run is
         * active, and both handlers execute on it.
         *
         * Returns why the loop ended: the caller's stop request, a server-side
         * removal, a receive timeout, or an escaped exception. Run does not
         * propagate exceptions from the session loop -- they arrive as
         * DisconnectReason::TransportError -- but a handler that throws is the
         * caller's own problem and does propagate.
         */
        DisconnectInfo
            Run( std::stop_token stopToken, SessionEventHandler eventHandler = {}, ConnectionCycleHandler cycleHandler = {} );

        /* Thread-safe wakeup for producers that queued work for the owner thread. */
        void Wake();

        void SendTextMessage( TextMessageTarget target, std::string_view text );
        void MoveToChannel( std::uint64_t channelId );
        void ChangeNickname( std::string_view nickname );
        void SetAudioState( AudioState state );
        void SendVoice( std::span<const std::byte> data, bool talkStart );
        void SendWhisper( const WhisperTarget& target, std::span<const std::byte> data, bool talkStart );
        void SendGroupWhisper( const GroupWhisper& target, std::span<const std::byte> data, bool talkStart );

        /* Always safe to call, in any state. */
        [[nodiscard]] ConnectionState State() const;
        [[nodiscard]] bool IsEstablished() const;

        /* The rest require an established session and throw NotConnectedError otherwise. */
        [[nodiscard]] std::uint16_t ClientId() const;
        [[nodiscard]] std::uint64_t CurrentChannelId() const;
        [[nodiscard]] std::string_view CurrentNickname() const;
        [[nodiscard]] std::string_view ServerName() const;
        [[nodiscard]] const ChannelStore& Channels() const;
        [[nodiscard]] const ClientStore& Clients() const;
        [[nodiscard]] ConnectionStatistics::Snapshot Statistics() const;

      private:
        void RequireEstablished() const;

        void DisconnectGracefully( std::string_view reason, SessionEventHandler& eventHandler );
        void DispatchEvents( SessionEventHandler& eventHandler );

        [[nodiscard]] std::optional<DisconnectInfo> SessionEnded() const;

        net::Address m_Address;
        net::UdpSocket m_Socket;

        Transport m_Transport;

        ClientProfile m_Profile;
        Identity m_Identity;

        std::uint64_t m_KeyOffset = 0;

        Handshake m_Handshake;

        ConnectionState m_State = ConnectionState::Created;

        std::unique_ptr<Session> m_Session;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CONNECTION_HPP
