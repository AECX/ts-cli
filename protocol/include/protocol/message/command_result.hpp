#ifndef TS_PROTOCOL_MESSAGE_COMMAND_RESULT_HPP
#define TS_PROTOCOL_MESSAGE_COMMAND_RESULT_HPP

#include <cstdint>
#include <optional>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    struct CommandResultEntry {
        std::uint32_t id = 0;
        std::string message;
        std::optional<std::string> returnCode;
    };

    class CommandResult {
      public:
        [[nodiscard]] static CommandResult Parse( const Command& command );

        [[nodiscard]] const std::vector<CommandResultEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        std::vector<CommandResultEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_COMMAND_RESULT_HPP
