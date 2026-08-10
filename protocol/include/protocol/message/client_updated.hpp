#ifndef TS_PROTOCOL_MESSAGE_CLIENT_UPDATED_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_UPDATED_HPP

#include <cstdint>
#include <optional>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct ClientUpdatedEntry {
        std::uint16_t id = 0;
        std::optional<std::string> nickname;
        std::optional<bool> away;
        std::optional<bool> inputMuted;
        std::optional<bool> outputMuted;
        std::optional<bool> inputHardware;
        std::optional<bool> outputHardware;
        std::optional<bool> recording;
        std::optional<bool> prioritySpeaker;
        std::optional<bool> channelCommander;
    };

    class ClientUpdated {
      public:
        [[nodiscard]] static ClientUpdated Parse( const Command& command );

        [[nodiscard]] const std::vector<ClientUpdatedEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );
        [[nodiscard]] static std::optional<bool> ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName );

        std::vector<ClientUpdatedEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_UPDATED_HPP
