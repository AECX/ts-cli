#include <log/terminal_color.hpp>
#include <optional>
#include <ostream>
#include <windows.h>

namespace ts::log {

    namespace {

        HANDLE ToHandle( TerminalStream target ) {
            return ::GetStdHandle( target == TerminalStream::StandardOutput ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE );
        }

        WORD ColorAttribute( TerminalColor color ) {
            switch ( color ) {
                case TerminalColor::BrightBlack:
                    return FOREGROUND_INTENSITY;

                case TerminalColor::Red:
                    return FOREGROUND_RED | FOREGROUND_INTENSITY;

                case TerminalColor::Green:
                    return FOREGROUND_GREEN | FOREGROUND_INTENSITY;

                case TerminalColor::Yellow:
                    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

                case TerminalColor::Magenta:
                    return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

                case TerminalColor::Cyan:
                    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            }

            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }

        /* Cached on first use, before anything ever changes it, so Reset can restore the console's true original colors. */
        WORD DefaultAttributes( TerminalStream target ) {
            static std::optional<WORD> cachedOutput;
            static std::optional<WORD> cachedError;

            std::optional<WORD>& cached = target == TerminalStream::StandardOutput ? cachedOutput : cachedError;

            if ( !cached ) {
                CONSOLE_SCREEN_BUFFER_INFO info {};

                cached = ::GetConsoleScreenBufferInfo( ToHandle( target ), &info ) != 0
                             ? info.wAttributes
                             : static_cast<WORD>( FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );
            }

            return *cached;
        }

    } // namespace

    void ApplyTerminalStyle( std::ostream& stream, TerminalStream target, TerminalStyle style ) {
        stream.flush();
        (void)DefaultAttributes( target ); // warm the cache before SetConsoleTextAttribute ever changes it
        ::SetConsoleTextAttribute( ToHandle( target ), ColorAttribute( style.color ) );
        /* Italic has no equivalent in the classic console attribute model; ignored. */
    }

    void ResetTerminalStyle( std::ostream& stream, TerminalStream target ) {
        stream.flush();
        ::SetConsoleTextAttribute( ToHandle( target ), DefaultAttributes( target ) );
    }

} // namespace ts::log
