#ifndef TS_CLIENT_RUNTIME_ACTION_HPP
#define TS_CLIENT_RUNTIME_ACTION_HPP

#include <client/cli/command.hpp>
#include <config/user_config.hpp>
#include <cstdint>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <string>
#include <variant>

namespace ts::client {

    struct SendCurrentChannelMessageAction {
        std::string text;
    };

    struct SendPrivateMessageAction {
        std::string target;
        std::string text;
    };

    /* Sends to an already-resolved target, e.g. a /r reply or a conversation picked in a GUI. */
    struct SendTextMessageAction {
        protocol::TextMessageTarget target;
        std::string text;
    };

    struct JoinChannelAction {
        std::string channel;
    };

    struct MoveToChannelAction {
        std::uint64_t channelId = 0;
    };

    struct ChangeNicknameAction {
        std::string nickname;
    };

    struct ListTreeAction {
        std::optional<std::string> start;
    };

    struct UserSettingsAction {
        std::string target;
        cli::UserCommandKind kind = cli::UserCommandKind::Inspect;
        float numberValue = 0.0F;
    };

    /* Replaces a remote client's persistent local playback settings without printing anything. */
    struct SetUserAudioAction {
        std::uint16_t clientId = 0;
        config::UserConfig settings;
    };

    using ClientAction = std::variant<SendCurrentChannelMessageAction,
                                      SendPrivateMessageAction,
                                      SendTextMessageAction,
                                      JoinChannelAction,
                                      MoveToChannelAction,
                                      ChangeNicknameAction,
                                      ListTreeAction,
                                      UserSettingsAction,
                                      SetUserAudioAction>;

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_ACTION_HPP
