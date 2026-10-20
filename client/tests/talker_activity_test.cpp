#include "test_support.hpp"

#include <chrono>
#include <client/runtime/talker_activity.hpp>
#include <cstdint>
#include <set>

namespace ts::client::test {

    void RunTalkerActivityTests() {
        TalkerActivity activity;
        const TalkerActivity::Clock::time_point start {};

        Expect( activity.Observe( 5, false, start ), "First voice packet did not start talking" );
        Expect( !activity.Observe( 5, false, start + std::chrono::milliseconds( 20 ) ),
                "Continuing voice packet reported a change" );
        Expect( activity.Observe( 6, false, start ), "Second talker did not start talking" );
        ExpectEqual( activity.Talking(), std::set<std::uint16_t> { 5, 6 }, "Talking set is wrong" );

        Expect( activity.Observe( 6, true, start + std::chrono::milliseconds( 40 ) ), "Talk-end did not stop talking" );
        Expect( !activity.Observe( 6, true, start + std::chrono::milliseconds( 60 ) ), "Repeated talk-end reported a change" );

        Expect( !activity.Expire( start + std::chrono::milliseconds( 20 ) + TalkerActivity::Timeout() / 2 ),
                "Talker expired before the timeout" );
        Expect( activity.Expire( start + std::chrono::milliseconds( 20 ) + TalkerActivity::Timeout() ),
                "Talker with a lost talk-end did not expire" );
        Expect( activity.Talking().empty(), "Expired talker is still talking" );
    }

} // namespace ts::client::test
