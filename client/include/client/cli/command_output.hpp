#ifndef TS_CLIENT_CLI_COMMAND_OUTPUT_HPP
#define TS_CLIENT_CLI_COMMAND_OUTPUT_HPP

#include <string_view>

namespace ts::client::cli {

    /*
     * Where CommandHandler reports local feedback (errors, confirmations,
     * help). The terminal Presentation implements it; other front ends (the
     * GTK client) provide their own so the same command set can be reused
     * without depending on terminal output.
     */
    class CommandOutput {
      public:
        virtual ~CommandOutput() = default;

        virtual void PrintError( std::string_view message ) = 0;
        virtual void PrintInfo( std::string_view message ) = 0;
        virtual void PrintHelp() = 0;
        virtual void Clear() = 0;

      protected:
        CommandOutput() = default;
        CommandOutput( const CommandOutput& ) = default;
        CommandOutput& operator=( const CommandOutput& ) = default;
    };

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_COMMAND_OUTPUT_HPP
