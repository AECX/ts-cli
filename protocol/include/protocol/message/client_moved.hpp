#ifndef TS_PROTOCOL_MESSAGE_CLIENT_MOVED_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_MOVED_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct ClientMovedEntry {
        std::uint16_t id = 0;
        std::uint64_t channelId = 0;
        std::uint64_t reasonId = 0;
        std::string reasonMessage;
    };

    class ClientMoved {
      public:
        [[nodiscard]] static ClientMoved Parse( const Command& command );

        [[nodiscard]] const std::vector<ClientMovedEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        std::vector<ClientMovedEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_MOVED_HPP
