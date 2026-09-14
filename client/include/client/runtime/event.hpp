#ifndef TS_CLIENT_RUNTIME_EVENT_HPP
#define TS_CLIENT_RUNTIME_EVENT_HPP

#include <cstdint>
#include <protocol/session/disconnect.hpp>
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

    /*
     * The session ended for a reason other than the user asking to leave --
     * a kick, a ban, a server shutdown or a receive timeout. Published so the
     * terminal says what happened instead of simply going quiet.
     */
    struct DisconnectedEvent {
        protocol::DisconnectInfo info;
    };

    using RuntimeEvent = std::variant<ProtocolEvent,
                                      ActionErrorEvent,
                                      ActionInfoEvent,
                                      CurrentChannelChangedEvent,
                                      CurrentNicknameChangedEvent,
                                      DisconnectedEvent>;

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_EVENT_HPP
