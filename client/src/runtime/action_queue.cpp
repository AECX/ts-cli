#include <client/runtime/action.hpp>
#include <client/runtime/action_queue.hpp>
#include <mutex>
#include <utility>

namespace ts::client {

    void ActionQueue::Push( ClientAction action ) {
        const std::lock_guard lock( m_Mutex );

        if ( m_Closed ) {
            return;
        }

        m_Actions.push_back( std::move( action ) );
    }

    bool ActionQueue::TryPop( ClientAction& action ) {
        const std::lock_guard lock( m_Mutex );

        if ( m_Actions.empty() ) {
            return false;
        }

        action = std::move( m_Actions.front() );
        m_Actions.pop_front();

        return true;
    }

    void ActionQueue::Close() {
        const std::lock_guard lock( m_Mutex );
        m_Closed = true;
    }

    bool ActionQueue::Closed() const {
        const std::lock_guard lock( m_Mutex );
        return m_Closed;
    }

} // namespace ts::client
