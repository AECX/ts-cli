#include <algorithm>
#include <audio/audio_types.hpp>
#include <client/config/user_config.hpp>
#include <client/runtime/action.hpp>
#include <client/runtime/action_processor.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <client/runtime/network_runtime.hpp>
#include <exception>
#include <map>
#include <optional>
#include <protocol/connection.hpp>
#include <protocol/session/event.hpp>
#include <protocol/state/channel.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client.hpp>
#include <protocol/state/client_store.hpp>
#include <set>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace ts::client {

    NetworkRuntime::NetworkRuntime( protocol::Connection& connection,
                                    ActionQueue& actionQueue,
                                    EventQueue& eventQueue,
                                    audio::AudioEngine& audio,
                                    UserConfigStore& userConfigStore ):
        m_Connection( connection ), m_ActionQueue( actionQueue ), m_EventQueue( eventQueue ), m_Audio( audio ),
        m_UserConfigStore( userConfigStore ), m_ActionProcessor( eventQueue, userConfigStore, audio ) {
        m_CurrentChannelId = m_Connection.CurrentChannelId();
        m_CurrentNickname = std::string( m_Connection.CurrentNickname() );
        SyncTalkerSettings( m_Connection );
    }

    NetworkRuntime::~NetworkRuntime() {
        RequestStop();

        if ( m_Thread.joinable() ) {
            m_Thread.join();
        }
    }

    void NetworkRuntime::Start() {
        if ( m_Thread.joinable() ) {
            throw std::runtime_error( "Network runtime is already running" );
        }

        m_Exception = nullptr;

        m_Thread = std::jthread( [this]( std::stop_token stopToken ) {
            Run( stopToken );
        } );
    }

    void NetworkRuntime::RequestStop() {
        if ( m_Thread.joinable() ) {
            m_Thread.request_stop();
            m_Connection.Wake();
        }
    }

    void NetworkRuntime::Join() {
        if ( !m_Thread.joinable() ) {
            throw std::runtime_error( "Network runtime is not running" );
        }

        m_Thread.join();

        if ( m_Exception ) {
            std::rethrow_exception( m_Exception );
        }
    }

    void NetworkRuntime::Run( std::stop_token stopToken ) {
        try {
            m_Connection.Run(
                stopToken,
                [this]( protocol::SessionEvent event ) {
                    PublishCurrentChannelIfChanged( m_Connection );
                    PublishCurrentNicknameIfChanged( m_Connection );
                    SyncTalkerSettings( m_Connection );

                    if ( !ProcessVoiceEvent( event ) ) {
                        m_EventQueue.Push( std::move( event ) );
                    }
                },
                [this]( protocol::Connection& connection ) {
                    PublishCurrentChannelIfChanged( connection );
                    PublishCurrentNicknameIfChanged( connection );
                    SyncTalkerSettings( connection );
                    ProcessActions( connection );
                    ProcessAudio( connection );
                } );
        } catch ( ... ) {
            m_Exception = std::current_exception();
        }

        m_ActionQueue.Close();
        m_EventQueue.Close();
    }

    void NetworkRuntime::ProcessActions( protocol::Connection& connection ) {
        ClientAction action;

        while ( m_ActionQueue.TryPop( action ) ) {
            m_ActionProcessor.Process( connection, action );
        }
    }

    void NetworkRuntime::ProcessAudio( protocol::Connection& connection ) {
        try {
            const audio::AudioStatus status = m_Audio.Status();
            const std::optional<audio::TransmitStateChange> transmitChange = m_Audio.TakeTransmitStateChange();

            if ( transmitChange ) {
                m_AudioTransmitEnabled = transmitChange->enabled;
                m_AudioTransmitRevision = transmitChange->revision;
            }

            if ( !m_AudioStatePublished || status.available != m_AudioAvailable || transmitChange ) {
                connection.SetAudioState( status.available, status.available, !m_AudioTransmitEnabled );
                m_AudioStatePublished = true;
                m_AudioAvailable = status.available;
            }

            audio::EncodedFrame frame;

            while ( m_Audio.TryPopOutgoing( frame ) ) {
                if ( frame.transmitRevision != m_AudioTransmitRevision ) {
                    continue;
                }

                if ( !m_AudioTransmitEnabled && !frame.talkEnd ) {
                    continue;
                }

                const std::span<const std::byte> data( frame.data.data(), frame.size );
                connection.SendVoice( data, frame.talkStart );
            }
        } catch ( const std::exception& exception ) {
            m_Audio.SetTransmitEnabled( false );
            m_EventQueue.Push( ActionErrorEvent { .message = std::string( "audio: " ) + exception.what() } );
        }
    }

    void NetworkRuntime::SyncTalkerSettings( protocol::Connection& connection ) {
        std::set<std::uint16_t> activeClientIds;

        for ( const protocol::Client* client : connection.Clients().All() ) {
            if ( client == nullptr || client->id == connection.ClientId() || !client->detailsKnown ||
                 client->uniqueId.empty() ) {
                continue;
            }

            activeClientIds.insert( client->id );
            const auto known = m_TalkerIdentities.find( client->id );
            if ( known != m_TalkerIdentities.end() && known->second == client->uniqueId ) {
                continue;
            }

            if ( known != m_TalkerIdentities.end() ) {
                m_Audio.RemoveTalker( client->id );
            }

            try {
                const UserConfig config = m_UserConfigStore.Load( client->uniqueId );
                m_Audio.SetTalkerSettings( client->id,
                                           audio::TalkerSettings { .volumeDb = config.volumeDb, .muted = config.muted } );
            } catch ( const std::exception& exception ) {
                m_Audio.SetTalkerSettings( client->id, audio::TalkerSettings {} );
                m_EventQueue.Push(
                    ActionErrorEvent { .message = "user settings for " + client->nickname + ": " + exception.what() } );
            }

            m_TalkerIdentities.insert_or_assign( client->id, client->uniqueId );
        }

        for ( auto known = m_TalkerIdentities.begin(); known != m_TalkerIdentities.end(); ) {
            if ( activeClientIds.contains( known->first ) ) {
                ++known;
                continue;
            }

            m_Audio.RemoveTalker( known->first );
            known = m_TalkerIdentities.erase( known );
        }
    }

    bool NetworkRuntime::ProcessVoiceEvent( const protocol::SessionEvent& event ) {
        const auto* voice = std::get_if<protocol::VoiceEvent>( &event );

        if ( voice == nullptr ) {
            return false;
        }

        if ( voice->frame.data.size() > audio::MaxEncodedBytes ) {
            return true;
        }

        audio::IncomingEncodedFrame frame;
        frame.clientId = voice->frame.clientId;
        frame.voiceId = voice->frame.voiceId;
        frame.codec = static_cast<std::uint8_t>( voice->frame.codec );
        frame.size = voice->frame.data.size();
        frame.talkStart = voice->frame.talkStart;
        frame.talkEnd = voice->frame.talkEnd;
        std::copy( voice->frame.data.begin(), voice->frame.data.end(), frame.data.begin() );

        (void)m_Audio.SubmitIncoming( frame );
        return true;
    }

    void NetworkRuntime::PublishCurrentChannelIfChanged( protocol::Connection& connection ) {
        const std::uint64_t channelId = connection.CurrentChannelId();

        if ( channelId == 0 || channelId == m_CurrentChannelId ) {
            return;
        }

        const protocol::Channel* channel = connection.Channels().Find( channelId );

        if ( channel == nullptr ) {
            return;
        }

        m_CurrentChannelId = channelId;
        m_EventQueue.Push( CurrentChannelChangedEvent { .channelId = channel->id, .name = channel->name } );
    }

    void NetworkRuntime::PublishCurrentNicknameIfChanged( protocol::Connection& connection ) {
        const std::string_view nickname = connection.CurrentNickname();

        if ( nickname.empty() || nickname == m_CurrentNickname ) {
            return;
        }

        m_CurrentNickname = std::string( nickname );
        m_EventQueue.Push( CurrentNicknameChangedEvent { .nickname = m_CurrentNickname } );
    }

} // namespace ts::client
