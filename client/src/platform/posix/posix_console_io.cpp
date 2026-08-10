#include <cerrno>
#include <chrono>
#include <client/platform/console_io.hpp>
#include <climits>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>

namespace ts::client::platform {

    bool StandardOutputIsTerminal() {
        return ::isatty( STDOUT_FILENO ) == 1;
    }

    bool StandardInputIsTerminal() {
        return ::isatty( STDIN_FILENO ) == 1;
    }

    bool StandardErrorIsTerminal() {
        return ::isatty( STDERR_FILENO ) == 1;
    }

    void InitializeConsole() {
        return;
    }

    StandardInputPoll PollStandardInputLine( std::chrono::milliseconds timeout ) {
        if ( timeout.count() < 0 || timeout.count() > INT_MAX ) {
            throw std::runtime_error( "Invalid standard input poll timeout" );
        }

        pollfd input { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 };

        const int result = ::poll( &input, 1, static_cast<int>( timeout.count() ) );

        if ( result < 0 ) {
            if ( errno == EINTR ) {
                return {};
            }

            throw std::runtime_error( "Failed to poll standard input" );
        }

        if ( result == 0 || ( input.revents & ( POLLIN | POLLHUP ) ) == 0 ) {
            return {};
        }

        std::string line;

        if ( !std::getline( std::cin, line ) ) {
            return { .status = StandardInputStatus::Closed, .line = {} };
        }

        return { .status = StandardInputStatus::Ready, .line = std::move( line ) };
    }

} // namespace ts::client::platform
