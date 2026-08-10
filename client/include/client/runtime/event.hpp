#ifndef TS_CLIENT_RUNTIME_EVENT_HPP
#define TS_CLIENT_RUNTIME_EVENT_HPP

#include <cstdint>
#include <protocol/session/event.hpp>
#include <string>
#include <variant>

namespace ts::client {

    struct ProtocolEvent {
        protocol::SessionEvent event;
    };

    struct ActionErrorEvent {
        std::string message;
    };

    struct ActionInfoEvent {
        std::string message;
    };

    struct CurrentChannelChangedEvent {
        std::uint64_t channelId = 0;
        std::string name;
    };

    struct CurrentNicknameChangedEvent {
        std::string nickname;
    };

    using RuntimeEvent =
        std::variant<ProtocolEvent, ActionErrorEvent, ActionInfoEvent, CurrentChannelChangedEvent, CurrentNicknameChangedEvent>;

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_EVENT_HPP
