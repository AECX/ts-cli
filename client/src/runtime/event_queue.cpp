#include <chrono>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <mutex>
#include <protocol/session/event.hpp>
#include <utility>

namespace ts::client {

    void EventQueue::Push( RuntimeEvent event ) {
        {
            const std::lock_guard lock( m_Mutex );

            if ( m_Closed ) {
                return;
            }

            m_Events.push_back( std::move( event ) );
        }

        m_Condition.notify_one();
    }

    void EventQueue::Push( protocol::SessionEvent event ) {
        Push( ProtocolEvent { .event = std::move( event ) } );
    }

    bool EventQueue::WaitPop( RuntimeEvent& event, std::chrono::milliseconds timeout ) {
        std::unique_lock lock( m_Mutex );

        m_Condition.wait_for( lock, timeout, [this]() {
            return m_Closed || !m_Events.empty();
        } );

        if ( m_Events.empty() ) {
            return false;
        }

        event = std::move( m_Events.front() );
        m_Events.pop_front();

        return true;
    }

    void EventQueue::Close() {
        {
            const std::lock_guard lock( m_Mutex );
            m_Closed = true;
        }

        m_Condition.notify_all();
    }

    bool EventQueue::Closed() const {
        const std::lock_guard lock( m_Mutex );
        return m_Closed;
    }

} // namespace ts::client
