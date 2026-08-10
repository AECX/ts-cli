#ifndef TS_LOG_OSTREAM_SINK_HPP
#define TS_LOG_OSTREAM_SINK_HPP

#include "sink.hpp"

#include <iosfwd>
#include <log/terminal_color.hpp>
#include <mutex>
#include <string_view>

namespace ts::log {

    class OstreamSink final: public Sink {
      public:
        explicit OstreamSink( std::ostream& stream, bool color = true, TerminalStream target = TerminalStream::StandardError );

        void Write( const Record& record ) override;

      private:
        [[nodiscard]] static std::string_view LevelName( Level level );

        [[nodiscard]] static TerminalStyle LevelStyle( Level level );

        std::ostream& m_Stream;
        bool m_Color;
        TerminalStream m_Target;
        std::mutex m_Mutex;
    };

} // namespace ts::log

#endif // TS_LOG_OSTREAM_SINK_HPP
