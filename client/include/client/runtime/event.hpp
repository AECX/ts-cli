#ifndef TS_CLIENT_RUNTIME_EVENT_HPP
#define TS_CLIENT_RUNTIME_EVENT_HPP

#include <client/runtime/server_snapshot.hpp>
#include <cstdint>
#include <protocol/session/disconnect.hpp>
#include <protocol/session/event.hpp>
#include <set>
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

    /*
     * The channel/client tree changed. Like TalkStateEvent, only published
     * when the runtime was asked for live state
     * (NetworkRuntime::EnableLiveStateEvents), i.e. by front ends that render
     * a live tree.
     */
    struct ServerSnapshotEvent {
        ServerSnapshot snapshot;
    };

    /* The set of clients currently transmitting voice changed; includes ourselves. */
    struct TalkStateEvent {
        std::set<std::uint16_t> talkingClientIds;
    };

    using RuntimeEvent = std::variant<ProtocolEvent,
                                      ActionErrorEvent,
                                      ActionInfoEvent,
                                      CurrentChannelChangedEvent,
                                      CurrentNicknameChangedEvent,
                                      DisconnectedEvent,
                                      ServerSnapshotEvent,
                                      TalkStateEvent>;

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_EVENT_HPP
