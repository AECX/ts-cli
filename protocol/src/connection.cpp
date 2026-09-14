#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <optional>
#include <protocol/connection.hpp>
#include <protocol/error.hpp>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <protocol/session/bootstrap.hpp>
#include <protocol/session/session.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace ts::protocol {

    Connection::Connection( ConnectionOptions options ):
        m_Address( net::ResolveEndpoint( net::ParseEndpoint( options.endpoint ) ).front() ), m_Socket( m_Address ),
        m_Transport( m_Socket ), m_Profile( std::move( options.profile ) ), m_Identity( std::move( options.identity ) ),
        m_KeyOffset( options.identityKeyOffset ),
        m_Handshake( m_Transport, m_Identity, m_Profile.version.initVersion, net::FormatHost( m_Address ) ) {
    }

    Connection::~Connection() = default;

    void Connection::Connect() {
        if ( m_State != ConnectionState::Created ) {
            throw ProtocolError( "Connection has already been connected" );
        }

        m_State = ConnectionState::Connecting;

        try {
            m_Socket.Connect();

            SessionBootstrap bootstrap = m_Handshake.Run();

            auto session = std::make_unique<Session>( m_Transport, std::move( bootstrap ) );
            session->Login( m_Profile, m_KeyOffset );

            m_Session = std::move( session );
        } catch ( ... ) {
            m_State = ConnectionState::Closed;

            throw;
        }

        m_State = ConnectionState::Established;
    }

    void Connection::Disconnect( std::string_view reason ) {
        RequireEstablished();

        SessionEventHandler discard;

        DisconnectGracefully( reason, discard );

        m_State = ConnectionState::Closed;
    }

    bool Connection::Poll( std::chrono::milliseconds timeout ) {
        RequireEstablished();

        m_Session->ProcessTimers();

        std::chrono::milliseconds wait = timeout;

        if ( const auto deadline = m_Session->NextDeadline() ) {
            const auto now = ReliableCommandQueue::Clock::now();

            if ( *deadline <= now ) {
                wait = std::chrono::milliseconds { 0 };
            } else {
                const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( *deadline - now );

                wait = std::min( wait, remaining );
            }
        }

        if ( !m_Transport.WaitReadable( wait ) ) {
            return false;
        }

        m_Session->ProcessPacket();

        return true;
    }

    bool Connection::HasEvent() const {
        RequireEstablished();

        return m_Session->HasEvent();
    }

    SessionEvent Connection::TakeEvent() {
        RequireEstablished();

        return m_Session->TakeEvent();
    }

    DisconnectInfo
        Connection::Run( std::stop_token stopToken, SessionEventHandler eventHandler, ConnectionCycleHandler cycleHandler ) {
        RequireEstablished();

        using namespace std::chrono_literals;

        const std::chrono::milliseconds maximumWait = cycleHandler ? 50ms : 250ms;

        DisconnectInfo info { .reason = DisconnectReason::LocalRequest, .serverReasonId = 0, .message = {} };

        try {
            /*
             * Commands may arrive while the initial state snapshot is being
             * synchronized. Deliver any events produced during that phase
             * before entering the normal live loop.
             */
            DispatchEvents( eventHandler );

            while ( !stopToken.stop_requested() ) {
                if ( cycleHandler ) {
                    cycleHandler();
                }

                (void)Poll( maximumWait );

                DispatchEvents( eventHandler );

                if ( const std::optional<DisconnectInfo> ended = SessionEnded() ) {
                    info = *ended;

                    break;
                }
            }

            /*
             * Only say goodbye when we are the ones leaving. After a server
             * removal there is nobody left to tell, and after a timeout the
             * packet would not arrive anyway.
             */
            if ( info.reason == DisconnectReason::LocalRequest ) {
                DisconnectGracefully( "Client disconnected", eventHandler );
            }
        } catch ( const std::exception& exception ) {
            m_State = ConnectionState::Closed;

            return DisconnectInfo { .reason = DisconnectReason::TransportError, .message = exception.what() };
        }

        m_State = ConnectionState::Closed;

        return info;
    }

    void Connection::Wake() {
        m_Socket.Wake();
    }

    void Connection::SendTextMessage( TextMessageTarget target, std::string_view text ) {
        RequireEstablished();

        m_Session->SendTextMessage( target, text );
    }

    void Connection::MoveToChannel( std::uint64_t channelId ) {
        RequireEstablished();

        m_Session->MoveToChannel( channelId );
    }

    void Connection::ChangeNickname( std::string_view nickname ) {
        RequireEstablished();

        m_Session->ChangeNickname( nickname );
    }

    void Connection::SetAudioState( AudioState state ) {
        RequireEstablished();

        m_Session->SetAudioState( state );
    }

    void Connection::SendVoice( std::span<const std::byte> data, bool talkStart ) {
        RequireEstablished();

        m_Session->SendVoice( data, talkStart );
    }

    void Connection::SendWhisper( const WhisperTarget& target, std::span<const std::byte> data, bool talkStart ) {
        RequireEstablished();

        m_Session->SendWhisper( target, data, talkStart );
    }

    void Connection::SendGroupWhisper( const GroupWhisper& target, std::span<const std::byte> data, bool talkStart ) {
        RequireEstablished();

        m_Session->SendGroupWhisper( target, data, talkStart );
    }

    void Connection::RequireEstablished() const {
        if ( m_State != ConnectionState::Established || !m_Session ) {
            throw NotConnectedError( "Connection is not established" );
        }
    }

    std::optional<DisconnectInfo> Connection::SessionEnded() const {
        if ( const std::optional<ServerDisconnect>& removal = m_Session->ServerRemoval() ) {
            return DisconnectInfo { .reason = DisconnectReason::ServerClosed,
                                    .serverReasonId = removal->reasonId,
                                    .message = removal->message };
        }

        if ( m_Session->TimedOut() ) {
            return DisconnectInfo { .reason = DisconnectReason::Timeout,
                                    .message = "No data received from the server for " +
                                               std::to_string( SessionTransport::ReceiveTimeout.count() ) + " seconds" };
        }

        return std::nullopt;
    }

    void Connection::DisconnectGracefully( std::string_view reason, SessionEventHandler& eventHandler ) {
        using namespace std::chrono_literals;

        constexpr std::chrono::milliseconds DisconnectTimeout = 1500ms;
        constexpr std::chrono::milliseconds MaximumWait = 100ms;

        m_Session->Disconnect( reason );

        const auto deadline = ReliableCommandQueue::Clock::now() + DisconnectTimeout;

        while ( m_Session->DisconnectPending() ) {
            const auto now = ReliableCommandQueue::Clock::now();

            if ( now >= deadline ) {
                break;
            }

            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( deadline - now );

            (void)Poll( std::min( MaximumWait, remaining ) );

            DispatchEvents( eventHandler );
        }
    }

    void Connection::DispatchEvents( SessionEventHandler& eventHandler ) {
        while ( m_Session->HasEvent() ) {
            SessionEvent event = m_Session->TakeEvent();

            if ( eventHandler ) {
                eventHandler( std::move( event ) );
            }
        }
    }

    ConnectionState Connection::State() const {
        return m_State;
    }

    bool Connection::IsEstablished() const {
        return m_State == ConnectionState::Established && m_Session != nullptr;
    }

    std::uint16_t Connection::ClientId() const {
        RequireEstablished();

        return m_Session->ClientId();
    }

    std::uint64_t Connection::CurrentChannelId() const {
        RequireEstablished();

        return m_Session->CurrentChannelId();
    }

    std::string_view Connection::CurrentNickname() const {
        RequireEstablished();

        return m_Session->CurrentNickname();
    }

    std::string_view Connection::ServerName() const {
        RequireEstablished();

        return m_Session->ServerName();
    }

    const ChannelStore& Connection::Channels() const {
        RequireEstablished();

        return m_Session->Channels();
    }

    const ClientStore& Connection::Clients() const {
        RequireEstablished();

        return m_Session->Clients();
    }

    ConnectionStatistics::Snapshot Connection::Statistics() const {
        RequireEstablished();

        return m_Session->Statistics();
    }

} // namespace ts::protocol
