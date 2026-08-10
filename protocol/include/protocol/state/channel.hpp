#ifndef TS_PROTOCOL_STATE_CHANNEL_HPP
#define TS_PROTOCOL_STATE_CHANNEL_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace ts::protocol {

    struct Channel {
        std::uint64_t id = 0;
        std::uint64_t parentId = 0;

        /*
         * TeamSpeak channel_order is the ID of the previous
         * sibling, not a numeric array index.
         *
         * Zero means this channel is the first child under
         * its parent.
         */
        std::uint64_t orderAfterId = 0;

        std::string name;

        bool permanent = false;
        bool semiPermanent = false;
        bool defaultChannel = false;
        bool passwordProtected = false;

        std::uint8_t codec = 4;
        bool codecIsUnencrypted = true;
        bool subscribed = false;
    };

    struct ChannelTreeEntry {
        const Channel* channel = nullptr;
        std::size_t depth = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_STATE_CHANNEL_HPP
