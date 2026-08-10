#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_LIST_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_LIST_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct ChannelListEntry {
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

    class ChannelList {
      public:
        [[nodiscard]] static ChannelList Parse( const Command& command );

        [[nodiscard]] const std::vector<ChannelListEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        [[nodiscard]] static bool ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName );

        std::vector<ChannelListEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_LIST_HPP
