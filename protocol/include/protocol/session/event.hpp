#ifndef TS_PROTOCOL_SESSION_EVENT_HPP
#define TS_PROTOCOL_SESSION_EVENT_HPP

#include <cstdint>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/voice/voice.hpp>
#include <string>
#include <variant>

namespace ts::protocol {

    struct TextMessageEvent {
        TextMessageEntry message;
        std::optional<TextMessageTarget> replyTarget;
        std::string channelName;
        std::string privatePeerName;
        bool outgoing = false;
    };

    enum class ClientPresenceKind { Joined, Left };

    struct ClientPresenceEvent {
        ClientPresenceKind kind = ClientPresenceKind::Joined;
        std::uint16_t clientId = 0;
        std::uint64_t channelId = 0;
        std::string clientName;
        std::string channelName;
    };

    struct CommandErrorEvent {
        std::uint32_t id = 0;
        std::string message;
    };

    struct VoiceEvent {
        VoiceFrame frame;
    };

    using SessionEvent = std::variant<TextMessageEvent, ClientPresenceEvent, CommandErrorEvent, VoiceEvent>;

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_EVENT_HPP
