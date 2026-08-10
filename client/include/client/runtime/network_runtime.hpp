#ifndef TS_CLIENT_RUNTIME_NETWORK_RUNTIME_HPP
#define TS_CLIENT_RUNTIME_NETWORK_RUNTIME_HPP

#include <audio/audio_engine.hpp>
#include <client/runtime/action_processor.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event_queue.hpp>
#include <cstdint>
#include <exception>
#include <map>
#include <string>
#include <thread>

namespace ts::protocol {
    class Connection;
}

namespace ts::client {

    class UserConfigStore;

    class NetworkRuntime {
      public:
        NetworkRuntime( protocol::Connection& connection,
                        ActionQueue& actionQueue,
                        EventQueue& eventQueue,
                        audio::AudioEngine& audio,
                        UserConfigStore& userConfigStore );

        ~NetworkRuntime();

        NetworkRuntime( const NetworkRuntime& ) = delete;
        NetworkRuntime& operator=( const NetworkRuntime& ) = delete;

        NetworkRuntime( NetworkRuntime&& ) = delete;
        NetworkRuntime& operator=( NetworkRuntime&& ) = delete;

        void Start();
        void RequestStop();
        void Join();

      private:
        void Run( std::stop_token stopToken );
        void ProcessActions( protocol::Connection& connection );
        void PublishCurrentChannelIfChanged( protocol::Connection& connection );
        void PublishCurrentNicknameIfChanged( protocol::Connection& connection );
        void ProcessAudio( protocol::Connection& connection );
        void SyncTalkerSettings( protocol::Connection& connection );
        bool ProcessVoiceEvent( const protocol::SessionEvent& event );

        protocol::Connection& m_Connection;
        ActionQueue& m_ActionQueue;
        EventQueue& m_EventQueue;
        audio::AudioEngine& m_Audio;
        UserConfigStore& m_UserConfigStore;
        ActionProcessor m_ActionProcessor;
        std::map<std::uint16_t, std::string> m_TalkerIdentities;
        std::uint64_t m_CurrentChannelId = 0;
        std::string m_CurrentNickname;
        bool m_AudioStatePublished = false;
        bool m_AudioAvailable = false;
        bool m_AudioTransmitEnabled = false;
        std::uint64_t m_AudioTransmitRevision = 0;

        std::jthread m_Thread;
        std::exception_ptr m_Exception;
    };

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_NETWORK_RUNTIME_HPP
