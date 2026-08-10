#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_MOVED_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_MOVED_HPP

#include <cstdint>
#include <protocol/command/command.hpp>

namespace ts::protocol {

    struct ChannelMovedEntry {
        std::uint64_t id = 0;
        std::uint64_t parentId = 0;
        std::uint64_t orderAfterId = 0;
    };

    class ChannelMoved {
      public:
        [[nodiscard]] static ChannelMoved Parse( const Command& command );
        [[nodiscard]] const ChannelMovedEntry& Entry() const;

      private:
        ChannelMovedEntry m_Entry;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_MOVED_HPP
