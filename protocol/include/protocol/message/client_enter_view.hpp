#ifndef TS_PROTOCOL_MESSAGE_CLIENT_ENTER_VIEW_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_ENTER_VIEW_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct ClientEnterViewEntry {
        std::uint16_t id = 0;
        std::uint64_t channelId = 0;

        std::string nickname;
        std::string uniqueId;

        bool away = false;
        bool inputMuted = false;
        bool outputMuted = false;
        bool inputHardware = true;
        bool outputHardware = true;
        bool recording = false;
        bool prioritySpeaker = false;
        bool channelCommander = false;
        bool serverQuery = false;
    };

    class ClientEnterView {
      public:
        [[nodiscard]] static ClientEnterView Parse( const Command& command );

        [[nodiscard]] const std::vector<ClientEnterViewEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        [[nodiscard]] static bool
            ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName, bool defaultValue = false );

        std::vector<ClientEnterViewEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_ENTER_VIEW_HPP
