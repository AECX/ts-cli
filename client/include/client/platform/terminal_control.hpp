#ifndef TS_CLIENT_PLATFORM_TERMINAL_CONTROL_HPP
#define TS_CLIENT_PLATFORM_TERMINAL_CONTROL_HPP

#include <ostream>

namespace ts::client::platform {

    /*
     * Interactive-prompt line control. `stream` must actually be the
     * process's std::cout -- it is used for output ordering (flushing
     * before a Windows console cursor/fill call) and, on POSIX, as the
     * destination for the escape bytes themselves.
     *
     * POSIX writes the equivalent ANSI cursor/erase sequences directly into
     * the stream. Windows calls the matching console cursor-position/fill
     * APIs instead, for the same reason log::ApplyTerminalStyle does:
     * embedded ANSI cursor sequences aren't reliably interpreted by the
     * classic Windows console or Wine's non-GUI console backend.
     */

    /* Returns to the start of the current line and erases it. */
    void ClearCurrentLine( std::ostream& stream );

    /* Moves up one line, then erases that line (used to remove an echoed input line). */
    void ClearPreviousLine( std::ostream& stream );

    /* Erases the entire visible screen. */
    void ClearScreen( std::ostream& stream );

} // namespace ts::client::platform

#endif // TS_CLIENT_PLATFORM_TERMINAL_CONTROL_HPP
