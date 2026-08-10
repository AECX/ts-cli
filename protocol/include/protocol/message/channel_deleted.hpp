#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_DELETED_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_DELETED_HPP

#include <cstdint>
#include <protocol/command/command.hpp>

namespace ts::protocol {

    class ChannelDeleted {
      public:
        [[nodiscard]] static ChannelDeleted Parse( const Command& command );
        [[nodiscard]] std::uint64_t ChannelId() const;

      private:
        std::uint64_t m_ChannelId = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_DELETED_HPP
