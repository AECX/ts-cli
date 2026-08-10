#ifndef TS_CLIENT_CLI_COMMAND_HANDLER_HPP
#define TS_CLIENT_CLI_COMMAND_HANDLER_HPP

#include <audio/audio_engine.hpp>
#include <client/cli/command.hpp>
#include <client/config/config.hpp>
#include <client/notification.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <functional>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <string_view>

namespace ts::client::cli {

    class Presentation;

    class CommandHandler {
      public:
        using PersistConfigCallback = std::function<void( const client::ConfigMutator& )>;

        CommandHandler( ActionQueue& actionQueue,
                        Presentation& presentation,
                        NotificationCallback notify,
                        audio::AudioEngine* audio = nullptr,
                        PersistConfigCallback persistConfig = {} );

        void Observe( const RuntimeEvent& event );

        /* Returns false when the caller should exit the interactive loop. */
        bool HandleLine( std::string_view line );

      private:
        void PersistAudioSettings();

        ActionQueue& m_ActionQueue;
        Presentation& m_Presentation;
        NotificationCallback m_Notify;
        std::optional<protocol::TextMessageTarget> m_ReplyTarget;
        audio::AudioEngine* m_Audio = nullptr;
        PersistConfigCallback m_PersistConfig;
    };

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_COMMAND_HANDLER_HPP
