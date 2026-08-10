#ifndef TS_CLIENT_RUNTIME_ACTION_HPP
#define TS_CLIENT_RUNTIME_ACTION_HPP

#include <client/cli/command.hpp>
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

    struct SendReplyMessageAction {
        protocol::TextMessageTarget target;
        std::string text;
    };

    struct JoinChannelAction {
        std::string channel;
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

    using ClientAction = std::variant<SendCurrentChannelMessageAction,
                                      SendPrivateMessageAction,
                                      SendReplyMessageAction,
                                      JoinChannelAction,
                                      ChangeNicknameAction,
                                      ListTreeAction,
                                      UserSettingsAction>;

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_ACTION_HPP
