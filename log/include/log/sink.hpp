#ifndef TS_LOG_SINK_HPP
#define TS_LOG_SINK_HPP

#include "record.hpp"

namespace ts::log {

    class Sink {
      public:
        virtual ~Sink() = default;

        virtual void Write( const Record& record ) = 0;
    };

    class NullSink final: public Sink {
      public:
        void Write( const Record& record ) override {
            (void)record;
        }
    };

} // namespace ts::log

#endif // TS_LOG_SINK_HPP
