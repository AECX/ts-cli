#include <client/platform/terminal_control.hpp>
#include <ostream>

namespace ts::client::platform {

    void ClearCurrentLine( std::ostream& stream ) {
        stream << "\r\x1b[2K" << std::flush;
    }

    void ClearPreviousLine( std::ostream& stream ) {
        stream << "\x1b[1A\r\x1b[2K" << std::flush;
    }

    void ClearScreen( std::ostream& stream ) {
        stream << "\x1b[2J" << std::flush;
    }

} // namespace ts::client::platform
