#include <log/terminal_color.hpp>
#include <ostream>
#include <string_view>

namespace ts::log {

    namespace {

        std::string_view AnsiColorCode( TerminalColor color ) {
            switch ( color ) {
                case TerminalColor::BrightBlack:
                    return "90";

                case TerminalColor::Red:
                    return "31";

                case TerminalColor::Green:
                    return "32";

                case TerminalColor::Yellow:
                    return "33";

                case TerminalColor::Magenta:
                    return "35";

                case TerminalColor::Cyan:
                    return "36";
            }

            return "39";
        }

    } // namespace

    void ApplyTerminalStyle( std::ostream& stream, TerminalStream /*target*/, TerminalStyle style ) {
        stream << "\x1b[" << AnsiColorCode( style.color );

        if ( style.italic ) {
            stream << ";3";
        }

        stream << 'm';
    }

    void ResetTerminalStyle( std::ostream& stream, TerminalStream /*target*/ ) {
        stream << "\x1b[0m";
    }

} // namespace ts::log
