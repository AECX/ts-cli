#ifndef TS_PROTOCOL_MESSAGE_CLIENT_LEFT_VIEW_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_LEFT_VIEW_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct ClientLeftViewEntry {
        std::uint16_t id = 0;
        std::uint64_t fromChannelId = 0;
        std::uint64_t toChannelId = 0;
        std::uint64_t reasonId = 0;
        std::string reasonMessage;
    };

    class ClientLeftView {
      public:
        [[nodiscard]] static ClientLeftView Parse( const Command& command );

        [[nodiscard]] const std::vector<ClientLeftViewEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        std::vector<ClientLeftViewEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_LEFT_VIEW_HPP
