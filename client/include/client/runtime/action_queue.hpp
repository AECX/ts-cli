#ifndef TS_CLIENT_RUNTIME_ACTION_QUEUE_HPP
#define TS_CLIENT_RUNTIME_ACTION_QUEUE_HPP

#include "action.hpp"

#include <deque>
#include <mutex>

namespace ts::client {

    class ActionQueue {
      public:
        void Push( ClientAction action );
        bool TryPop( ClientAction& action );
        void Close();

        [[nodiscard]] bool Closed() const;

      private:
        mutable std::mutex m_Mutex;
        std::deque<ClientAction> m_Actions;
        bool m_Closed = false;
    };

} // namespace ts::client

#endif // TS_CLIENT_RUNTIME_ACTION_QUEUE_HPP
