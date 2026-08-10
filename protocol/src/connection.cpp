#include <algorithm>
#include <chrono>
#include <memory>
#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <protocol/connection.hpp>
#include <protocol/reliability/reliable_command_queue.hpp>
#include <protocol/session/bootstrap.hpp>
#include <protocol/session/session.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <utility>

namespace ts::protocol {

    Connection::Connection( std::string_view endpoint, ClientProfile profile, Identity identity, std::uint64_t keyOffset ):
        m_Address( net::ResolveEndpoint( net::ParseEndpoint( endpoint ) ).front() ), m_Socket( m_Address ),
        m_Transport( m_Socket ), m_Profile( std::move( profile ) ), m_Identity( std::move( identity ) ),
        m_KeyOffset( keyOffset ),
        m_Handshake( m_Transport, m_Identity, m_Profile.version.initVersion, net::FormatHost( m_Address ) ) {
    }

    Connection::~Connection() = default;

    void Connection::Connect() {
        m_Socket.Connect();

        SessionBootstrap bootstrap = m_Handshake.Run();

        auto session = std::make_unique<Session>( m_Transport, std::move( bootstrap ) );
        session->Login( m_Profile, m_KeyOffset );

        m_Session = std::move( session );
    }

    void Connection::Run( std::stop_token stopToken, SessionEventHandler eventHandler, ConnectionCycleHandler cycleHandler ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        using namespace std::chrono_literals;

        const std::chrono::milliseconds maximumWait = cycleHandler ? 50ms : 250ms;

        /*
         * Commands may arrive while the initial state snapshot is being
         * synchronized. Deliver any events produced during that phase before
         * entering the normal live loop.
         */
        DispatchEvents( eventHandler );

        while ( !stopToken.stop_requested() ) {
            if ( cycleHandler ) {
                cycleHandler( *this );
            }

            ProcessOnce( maximumWait, eventHandler );
        }

        DisconnectGracefully( eventHandler );
    }

    void Connection::Wake() {
        m_Socket.Wake();
    }

    void Connection::SendTextMessage( TextMessageTarget target, std::string_view text ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        m_Session->SendTextMessage( target, text );
    }

    void Connection::MoveToChannel( std::uint64_t channelId ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        m_Session->MoveToChannel( channelId );
    }

    void Connection::ChangeNickname( std::string_view nickname ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        m_Session->ChangeNickname( nickname );
    }

    void Connection::SetAudioState( bool inputHardware, bool outputHardware, bool inputMuted ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        m_Session->SetAudioState( inputHardware, outputHardware, inputMuted );
    }

    void Connection::SendVoice( std::span<const std::byte> data, bool talkStart ) {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        m_Session->SendVoice( data, talkStart );
    }

    void Connection::ProcessOnce( std::chrono::milliseconds maximumWait, SessionEventHandler& eventHandler ) {
        m_Session->ProcessTimers();

        std::chrono::milliseconds wait = maximumWait;

        if ( const auto deadline = m_Session->NextDeadline() ) {
            const auto now = ReliableCommandQueue::Clock::now();

            if ( *deadline <= now ) {
                wait = std::chrono::milliseconds { 0 };
            } else {
                const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( *deadline - now );

                wait = std::min( wait, remaining );
            }
        }

        if ( m_Transport.WaitReadable( wait ) ) {
            m_Session->ProcessPacket();
        }

        DispatchEvents( eventHandler );
    }

    void Connection::DisconnectGracefully( SessionEventHandler& eventHandler ) {
        using namespace std::chrono_literals;

        constexpr std::chrono::milliseconds DisconnectTimeout = 1500ms;
        constexpr std::chrono::milliseconds MaximumWait = 100ms;

        m_Session->Disconnect( "Client disconnected" );

        const auto deadline = ReliableCommandQueue::Clock::now() + DisconnectTimeout;

        while ( m_Session->DisconnectPending() ) {
            const auto now = ReliableCommandQueue::Clock::now();

            if ( now >= deadline ) {
                break;
            }

            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( deadline - now );

            ProcessOnce( std::min( MaximumWait, remaining ), eventHandler );
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

    std::uint16_t Connection::ClientId() const {
        if ( !m_Session ) {
            return 0;
        }

        return m_Session->ClientId();
    }

    std::uint64_t Connection::CurrentChannelId() const {
        if ( !m_Session ) {
            return 0;
        }

        return m_Session->CurrentChannelId();
    }

    std::string_view Connection::CurrentNickname() const {
        if ( !m_Session ) {
            return {};
        }

        return m_Session->CurrentNickname();
    }

    std::string_view Connection::ServerName() const {
        if ( !m_Session ) {
            return {};
        }

        return m_Session->ServerName();
    }

    const ChannelStore& Connection::Channels() const {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        return m_Session->Channels();
    }

    const ClientStore& Connection::Clients() const {
        if ( !m_Session ) {
            throw std::runtime_error( "Connection is not established" );
        }

        return m_Session->Clients();
    }

} // namespace ts::protocol
