#ifndef TS_LOG_TERMINAL_COLOR_HPP
#define TS_LOG_TERMINAL_COLOR_HPP

#include <ostream>

namespace ts::log {

    enum class TerminalColor {
        BrightBlack,
        Red,
        Green,
        Yellow,
        Magenta,
        Cyan,
    };

    enum class TerminalStream {
        StandardOutput,
        StandardError,
    };

    struct TerminalStyle {
        TerminalColor color;
        bool italic = false;
    };

    /*
     * Applies/resets a foreground color (and, on POSIX, italic) on the
     * console behind `target`. `stream` must actually be std::cout for
     * StandardOutput or std::cerr for StandardError -- it is used for
     * output ordering (flushing before a Windows console-attribute change)
     * and, on POSIX, as the destination for the escape bytes themselves.
     *
     * POSIX writes ANSI SGR escapes directly into the stream. Windows calls
     * SetConsoleTextAttribute on the matching console handle instead of
     * embedding escape bytes, since neither the classic Windows console nor
     * Wine's non-GUI console backend reliably parses embedded ANSI codes.
     * Consequently, on Windows this has no visible effect on an arbitrary
     * non-console ostream such as a test's std::ostringstream -- only on
     * the real console.
     */
    void ApplyTerminalStyle( std::ostream& stream, TerminalStream target, TerminalStyle style );
    void ResetTerminalStyle( std::ostream& stream, TerminalStream target );

} // namespace ts::log

#endif // TS_LOG_TERMINAL_COLOR_HPP
