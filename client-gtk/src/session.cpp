#include <audio/audio_engine.hpp>
#include <client/gtk/session.hpp>
#include <config/user_config.hpp>
#include <exception>
#include <memory>
#include <mutex>
#include <protocol/connection.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace ts::client::gtk {

    void NetworkWake::Set( protocol::Connection* connection ) {
        const std::scoped_lock lock( m_Mutex );
        m_Connection = connection;
    }

    void NetworkWake::Wake() {
        const std::scoped_lock lock( m_Mutex );

        if ( m_Connection != nullptr ) {
            m_Connection->Wake();
        }
    }

    Session::Session( protocol::ConnectionOptions options,
                      audio::AudioEngine& audio,
                      config::UserConfigStore& userConfigStore,
                      NetworkWake& wake ): m_Audio( audio ), m_UserConfigStore( userConfigStore ), m_Wake( wake ) {
        /* Constructing the connection resolves the endpoint, which can block as well. */
        m_Connector = std::thread( [this, options = std::move( options )]() mutable {
            try {
                m_Connection = std::make_unique<protocol::Connection>( std::move( options ) );
                m_Connection->Connect();

                if ( m_Abandoned ) {
                    m_Connection->Disconnect();
                    m_Error = "Connecting was cancelled";
                }
            } catch ( const std::exception& exception ) {
                m_Error = exception.what();
            }

            m_ConnectFinished = true;
        } );
    }

    Session::~Session() {
        if ( m_Runtime ) {
            m_Runtime.reset();
        }

        if ( m_Connector.joinable() ) {
            m_Connector.join();
        }

        m_Wake.Set( nullptr );
    }

    Session::State Session::Poll() {
        if ( m_State != State::Connecting || !m_ConnectFinished ) {
            return m_State;
        }

        m_Connector.join();

        if ( !m_Error.empty() ) {
            m_State = State::Failed;
            return m_State;
        }

        if ( m_Abandoned ) {
            /* Cancelled just as the connect attempt succeeded. */
            try {
                m_Connection->Disconnect();
            } catch ( const std::exception& ) {
                /* Leaving anyway; the server times the client out otherwise. */
            }

            m_Error = "Connecting was cancelled";
            m_State = State::Failed;
            return m_State;
        }

        try {
            m_Runtime.emplace( *m_Connection, m_Actions, m_Events, m_Audio, m_UserConfigStore );
            m_Runtime->EnableLiveStateEvents();
            m_Wake.Set( m_Connection.get() );
            m_Runtime->Start();
            m_State = State::Connected;
        } catch ( const std::exception& exception ) {
            m_Wake.Set( nullptr );
            m_Runtime.reset();
            m_Error = exception.what();
            m_State = State::Failed;
        }

        return m_State;
    }

    const std::string& Session::Error() const {
        return m_Error;
    }

    void Session::RequestStop() {
        if ( m_Runtime ) {
            m_Runtime->RequestStop();
        } else {
            m_Abandoned = true;
        }
    }

    bool Session::Ended() const {
        return m_State == State::Connected && m_Events.Closed();
    }

    void Session::Join() {
        if ( !m_Runtime ) {
            throw std::runtime_error( "Session is not connected" );
        }

        m_Wake.Set( nullptr );
        m_Runtime->Join();
    }

    ActionQueue& Session::Actions() {
        return m_Actions;
    }

    EventQueue& Session::Events() {
        return m_Events;
    }

} // namespace ts::client::gtk
