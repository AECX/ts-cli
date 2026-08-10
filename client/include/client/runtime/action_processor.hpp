#ifndef TS_CLIENT_RUNTIME_ACTION_PROCESSOR_HPP
#define TS_CLIENT_RUNTIME_ACTION_PROCESSOR_HPP

#include "action.hpp"

#include <client/cli/target_resolver.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ts::audio {
    class AudioEngine;
}

namespace ts::protocol {
    class Connection;
} // namespace ts::protocol

namespace ts::client {

    class EventQueue;
    class UserConfigStore;

    class ActionProcessor {
      public:
        ActionProcessor( EventQueue& eventQueue, UserConfigStore& userConfigStore, audio::AudioEngine& audio );

        void Process( protocol::Connection& connection, const ClientAction& action );

      private:
        [[nodiscard]] static std::vector<cli::TargetCandidate> ClientCandidates( const protocol::Connection& connection );
        [[nodiscard]] static std::vector<cli::TargetCandidate> ChannelCandidates( const protocol::Connection& connection );

        void ProcessListTree( protocol::Connection& connection, const ListTreeAction& action );
        void ProcessUserSettings( protocol::Connection& connection, const UserSettingsAction& action );
        void PushError( std::string message );
        void PushInfo( std::string message );

        EventQueue& m_EventQueue;
        UserConfigStore& m_UserConfigStore;
        audio::AudioEngine& m_Audio;
    };

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_ACTION_PROCESSOR_HPP
