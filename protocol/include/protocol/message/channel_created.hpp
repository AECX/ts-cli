#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_CREATED_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_CREATED_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>

namespace ts::protocol {

    struct ChannelCreatedEntry {
        std::uint64_t id = 0;
        std::uint64_t parentId = 0;
        std::uint64_t orderAfterId = 0;
        std::string name;
        bool permanent = false;
        bool semiPermanent = false;
        bool defaultChannel = false;
        bool passwordProtected = false;
        std::uint8_t codec = 4;
        bool codecIsUnencrypted = true;
    };

    class ChannelCreated {
      public:
        [[nodiscard]] static ChannelCreated Parse( const Command& command );
        [[nodiscard]] const ChannelCreatedEntry& Entry() const;

      private:
        ChannelCreatedEntry m_Entry;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_CREATED_HPP
