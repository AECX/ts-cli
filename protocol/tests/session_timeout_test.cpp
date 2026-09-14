#include "test_support.hpp"

#include <chrono>
#include <protocol/session_transport.hpp>

namespace ts::test {

    void RunSessionTimeoutTests() {
        using SessionTransport = protocol::SessionTransport;
        using Clock = SessionTransport::Clock;

        const Clock::time_point received = Clock::now();

        constexpr auto timeout = SessionTransport::ReceiveTimeout;

        Expect( !SessionTransport::IsTimedOut( received, received ), "A fresh receive must not count as a timeout" );

        Expect( !SessionTransport::IsTimedOut( received, received + timeout - std::chrono::seconds { 1 } ),
                "Silence shorter than the timeout must not count as a timeout" );

        /*
         * The boundary is inclusive: exactly ReceiveTimeout of silence is a
         * timeout, so the reported reason matches the documented constant.
         */
        Expect( SessionTransport::IsTimedOut( received, received + timeout ),
                "Silence of exactly the timeout must count as a timeout" );

        Expect( SessionTransport::IsTimedOut( received, received + timeout + std::chrono::seconds { 5 } ),
                "Silence longer than the timeout must count as a timeout" );

        /* The server is pinged once a second, so the window must clear that comfortably. */
        Expect( timeout >= std::chrono::seconds { 10 }, "The receive timeout must be well above the ping interval" );

        /* A receive timestamp in the future must never be read as a long silence. */
        Expect( !SessionTransport::IsTimedOut( received + timeout, received ),
                "A receive timestamp in the future must not count as a timeout" );
    }

} // namespace ts::test
