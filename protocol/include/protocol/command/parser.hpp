#ifndef TS_PROTOCOL_COMMAND_PARSER_HPP
#define TS_PROTOCOL_COMMAND_PARSER_HPP

#include "command.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace ts::protocol {

    class CommandParser {
      public:
        [[nodiscard]] static Command Parse( std::span<const std::byte> data );

        [[nodiscard]] static Command Parse( std::string_view text );

      private:
        [[nodiscard]] static CommandRow ParseRow( std::string_view text );

        [[nodiscard]] static std::string DecodeValue( std::string_view value );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_COMMAND_PARSER_HPP
