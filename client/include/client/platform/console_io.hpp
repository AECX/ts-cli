#ifndef TS_CLIENT_PLATFORM_CONSOLE_IO_HPP
#define TS_CLIENT_PLATFORM_CONSOLE_IO_HPP

#include <chrono>
#include <string>

namespace ts::client::platform {

    /*
     * Interactive-terminal detection and line input, behind a stable
     * interface. Only the implementation differs between build targets
     * (POSIX today, Win32 later), so the interactive session loop never
     * depends on OS console headers.
     */
    [[nodiscard]] bool StandardOutputIsTerminal();
    [[nodiscard]] bool StandardInputIsTerminal();
    [[nodiscard]] bool StandardErrorIsTerminal();

    /*
     * One time initialization code to set up
     * the terminal as needed for the target.
     */
    void InitializeConsole();

    enum class StandardInputStatus {
        Timeout, // no complete line arrived within the timeout
        Ready,   // `line` holds one complete line
        Closed,  // standard input reached end-of-file
    };

    struct StandardInputPoll {
        StandardInputStatus status = StandardInputStatus::Timeout;
        std::string line;
    };

    /*
     * Waits up to `timeout` for one complete, newline-terminated line of
     * standard input, so the caller can keep draining other event sources
     * between polls.
     *
     * ts-cli is an interactive-only client (piping/scripting input into it
     * is intentionally unsupported), so this only ever has to handle a real
     * console. That still isn't quite the same problem on every platform:
     * POSIX's poll() on a tty only reports readable once the terminal's
     * line discipline has buffered a complete line, so the POSIX backend
     * can poll and then read directly. A Win32 console handle instead
     * signals on every individual input record (keystrokes, mode changes,
     * ...), not just completed lines, so the Win32 backend runs the actual
     * blocking read on a dedicated background thread and reports a line
     * here only once one has actually been read.
     */
    [[nodiscard]] StandardInputPoll PollStandardInputLine( std::chrono::milliseconds timeout );

} // namespace ts::client::platform

#endif // TS_CLIENT_PLATFORM_CONSOLE_IO_HPP
