#ifndef TS_PROTOCOL_MESSAGE_CHANNEL_EDITED_HPP
#define TS_PROTOCOL_MESSAGE_CHANNEL_EDITED_HPP

#include <cstdint>
#include <optional>
#include <protocol/command/command.hpp>
#include <string>

namespace ts::protocol {

    struct ChannelEditedEntry {
        std::uint64_t id = 0;
        std::optional<std::uint64_t> parentId;
        std::optional<std::uint64_t> orderAfterId;
        std::optional<std::string> name;
        std::optional<bool> permanent;
        std::optional<bool> semiPermanent;
        std::optional<bool> defaultChannel;
        std::optional<bool> passwordProtected;
        std::optional<std::uint8_t> codec;
        std::optional<bool> codecIsUnencrypted;
    };

    class ChannelEdited {
      public:
        [[nodiscard]] static ChannelEdited Parse( const Command& command );
        [[nodiscard]] const ChannelEditedEntry& Entry() const;

      private:
        ChannelEditedEntry m_Entry;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CHANNEL_EDITED_HPP
