#ifndef TS_CLIENT_CLI_COMMAND_HPP
#define TS_CLIENT_CLI_COMMAND_HPP

#include <optional>
#include <string>
#include <variant>

namespace ts::client::cli {

    struct SendChannelCommand {
        std::string text;
    };

    struct PrivateMessageCommand {
        std::string target;
        std::string text;
    };

    struct ReplyCommand {
        std::string text;
    };

    struct JoinCommand {
        std::string channel;
    };

    struct NickCommand {
        std::string nickname;
    };

    struct ListCommand {
        std::optional<std::string> start;
    };

    enum class AudioCommandKind { Devices, Status, Input, Output, Filter, Threshold, Transmit };

    enum class UserCommandKind { Inspect, VolumeDb, VolumePercent, VolumeReset, Mute, Unmute };

    struct UserCommand {
        std::string target;
        UserCommandKind kind = UserCommandKind::Inspect;
        float numberValue = 0.0F;
    };

    struct AudioCommand {
        AudioCommandKind kind = AudioCommandKind::Status;
        std::string value;
        float numberValue = 0.0F;
    };

    struct ClearCommand {};

    struct HelpCommand {};

    struct QuitCommand {};

    using InputCommand = std::variant<SendChannelCommand,
                                      PrivateMessageCommand,
                                      ReplyCommand,
                                      JoinCommand,
                                      NickCommand,
                                      ListCommand,
                                      UserCommand,
                                      AudioCommand,
                                      ClearCommand,
                                      HelpCommand,
                                      QuitCommand>;

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_COMMAND_HPP
