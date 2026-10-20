#include <chrono>
#include <client/runtime/talker_activity.hpp>
#include <cstdint>
#include <set>

namespace ts::client {

    bool TalkerActivity::Observe( std::uint16_t clientId, bool talkEnd, Clock::time_point now ) {
        if ( talkEnd ) {
            return m_LastPacket.erase( clientId ) != 0;
        }

        const auto [entry, inserted] = m_LastPacket.insert_or_assign( clientId, now );
        (void)entry;
        return inserted;
    }

    bool TalkerActivity::Expire( Clock::time_point now ) {
        bool changed = false;

        for ( auto entry = m_LastPacket.begin(); entry != m_LastPacket.end(); ) {
            if ( now - entry->second < Timeout() ) {
                ++entry;
                continue;
            }

            entry = m_LastPacket.erase( entry );
            changed = true;
        }

        return changed;
    }

    std::set<std::uint16_t> TalkerActivity::Talking() const {
        std::set<std::uint16_t> result;

        for ( const auto& [clientId, lastPacket] : m_LastPacket ) {
            result.insert( clientId );
        }

        return result;
    }

    TalkerActivity::Clock::duration TalkerActivity::Timeout() {
        return std::chrono::milliseconds( 500 );
    }

} // namespace ts::client
