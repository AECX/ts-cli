#ifndef TS_CLIENT_RUNTIME_EVENT_QUEUE_HPP
#define TS_CLIENT_RUNTIME_EVENT_QUEUE_HPP

#include "event.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <protocol/session/event.hpp>

namespace ts::client {

    class EventQueue {
      public:
        void Push( RuntimeEvent event );
        void Push( protocol::SessionEvent event );

        bool WaitPop( RuntimeEvent& event, std::chrono::milliseconds timeout );

        void Close();

        [[nodiscard]] bool Closed() const;

      private:
        mutable std::mutex m_Mutex;
        std::condition_variable m_Condition;

        std::deque<RuntimeEvent> m_Events;
        bool m_Closed = false;
    };

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_EVENT_QUEUE_HPP
