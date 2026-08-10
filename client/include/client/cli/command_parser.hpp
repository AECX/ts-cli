#ifndef TS_CLIENT_CLI_COMMAND_PARSER_HPP
#define TS_CLIENT_CLI_COMMAND_PARSER_HPP

#include "command.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace ts::client::cli {

    class CommandParser {
      public:
        [[nodiscard]] static std::optional<InputCommand> Parse( std::string_view line );

      private:
        struct ParsedArgument {
            std::string value;
            std::string_view remainder;
        };

        [[nodiscard]] static std::string_view Trim( std::string_view value );
        [[nodiscard]] static std::string LowerAscii( std::string_view value );

        [[nodiscard]] static ParsedArgument ParseArgument( std::string_view value );

        [[nodiscard]] static std::string ParseWholeValue( std::string_view value, std::string_view usage );
        [[nodiscard]] static float ParseFloatValue( std::string_view value, std::string_view usage );

        [[nodiscard]] static InputCommand ParsePrivateMessage( std::string_view arguments );
    };

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_COMMAND_PARSER_HPP
