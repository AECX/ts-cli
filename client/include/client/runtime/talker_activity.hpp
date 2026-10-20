#ifndef TS_CLIENT_RUNTIME_TALKER_ACTIVITY_HPP
#define TS_CLIENT_RUNTIME_TALKER_ACTIVITY_HPP

#include <chrono>
#include <cstdint>
#include <map>
#include <set>

namespace ts::client {

    /*
     * Who is currently transmitting, derived from voice traffic: a client
     * talks from its first voice packet until its talk-end packet, or until
     * no packet arrived for Timeout() (talk-end packets can be lost).
     */
    class TalkerActivity {
      public:
        using Clock = std::chrono::steady_clock;

        /* Each returns true when the set of talking clients changed. */
        bool Observe( std::uint16_t clientId, bool talkEnd, Clock::time_point now );
        bool Expire( Clock::time_point now );

        [[nodiscard]] std::set<std::uint16_t> Talking() const;

        [[nodiscard]] static Clock::duration Timeout();

      private:
        std::map<std::uint16_t, Clock::time_point> m_LastPacket;
    };

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_TALKER_ACTIVITY_HPP
