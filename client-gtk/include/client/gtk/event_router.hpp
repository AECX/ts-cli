#ifndef TS_CLIENT_GTK_EVENT_ROUTER_HPP
#define TS_CLIENT_GTK_EVENT_ROUTER_HPP

#include "conversation_log.hpp"

#include <audio/audio_types.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/server_snapshot.hpp>
#include <optional>
#include <vector>

namespace ts::client::gtk {

    struct RoutedLine {
        /* std::nullopt: whatever conversation the user is currently looking at. */
        std::optional<ConversationKey> conversation;
        FeedLine line;
    };

    struct RoutedEvent {
        std::vector<RoutedLine> lines;
        std::optional<audio::NotificationType> notification;
    };

    /*
     * Decides which conversation feed(s) a runtime event belongs to and how it
     * reads there. Text messages land in their channel, private or server
     * conversation; local feedback (errors, command output) lands in the
     * active one. `snapshot` supplies fallbacks such as the current channel.
     */
    [[nodiscard]] RoutedEvent RouteEvent( const RuntimeEvent& event, const ServerSnapshot& snapshot );

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_EVENT_ROUTER_HPP
