#ifndef TS_CLIENT_CLI_CHANNEL_TREE_VIEW_HPP
#define TS_CLIENT_CLI_CHANNEL_TREE_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ts::protocol {
    struct ChannelTreeEntry;
    struct Client;
    class ClientStore;
} // namespace ts::protocol

namespace ts::client::cli {

    struct ChannelTreeRange {
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t baseDepth = 0;

        /*
         * True when `begin` is a single, explicitly selected channel
         * (e.g. "/list <channel>") rather than one of possibly several
         * top-level channels. Mirrors how `tree(1)` prints its argument
         * directory bare and only draws branches for its contents.
         */
        bool rootIsUnadorned = false;
    };

    /*
     * Renders channels and their clients as a `tree(1)`-style listing
     * (branch/elbow connectors, vertical continuation bars) for the
     * given sub-range of an already-flattened, depth-first channel tree.
     */
    [[nodiscard]] std::vector<std::string> FormatChannelTree( const std::vector<protocol::ChannelTreeEntry>& tree,
                                                              const ChannelTreeRange& range,
                                                              const protocol::ClientStore& clients,
                                                              std::uint16_t selfClientId );

    [[nodiscard]] std::string FormatClientStatus( const protocol::Client& client, std::uint16_t selfId );

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_CHANNEL_TREE_VIEW_HPP
