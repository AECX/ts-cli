#ifndef TS_CLIENT_RUNTIME_SERVER_SNAPSHOT_HPP
#define TS_CLIENT_RUNTIME_SERVER_SNAPSHOT_HPP

#include <cstddef>
#include <cstdint>
#include <protocol/state/channel.hpp>
#include <protocol/state/client.hpp>
#include <set>
#include <string>
#include <vector>

namespace ts::protocol {
    class Connection;
}

namespace ts::client {

    struct ChannelSnapshot {
        protocol::Channel channel;
        std::size_t depth = 0;

        bool operator==( const ChannelSnapshot& ) const = default;
    };

    /*
     * Self-contained copy of the server view owned by the network thread, so
     * a UI thread can render the channel/client tree without touching
     * protocol state. Channels are in depth-first display order.
     */
    struct ServerSnapshot {
        std::string serverName;
        std::uint16_t clientId = 0;
        std::uint64_t currentChannelId = 0;
        std::vector<ChannelSnapshot> channels;
        std::vector<protocol::Client> clients;
        /* Clients muted for local playback by their per-user settings. */
        std::set<std::uint16_t> locallyMutedClientIds;

        bool operator==( const ServerSnapshot& ) const = default;
    };

    [[nodiscard]] ServerSnapshot CaptureServerSnapshot( const protocol::Connection& connection );

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_SERVER_SNAPSHOT_HPP
