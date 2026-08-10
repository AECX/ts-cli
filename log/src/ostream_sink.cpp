#include <log/ostream_sink.hpp>
#include <ostream>

namespace ts::log {

    OstreamSink::OstreamSink( std::ostream& stream, bool color, TerminalStream target ):
        m_Stream( stream ), m_Color( color ), m_Target( target ) {
    }

    void OstreamSink::Write( const Record& record ) {
        const std::lock_guard lock( m_Mutex );

        if ( m_Color ) {
            ApplyTerminalStyle( m_Stream, m_Target, LevelStyle( record.level ) );
        }

        m_Stream << '[' << LevelName( record.level ) << "] [" << record.component << "] " << record.message;

        if ( m_Color ) {
            ResetTerminalStyle( m_Stream, m_Target );
        }

        m_Stream << std::endl;
    }

    std::string_view OstreamSink::LevelName( Level level ) {
        switch ( level ) {
            case Level::Trace:
                return "trace";

            case Level::Debug:
                return "debug";

            case Level::Info:
                return "info";

            case Level::Warning:
                return "warning";

            case Level::Error:
                return "error";

            case Level::Off:
                return "off";
        }

        return "unknown";
    }

    TerminalStyle OstreamSink::LevelStyle( Level level ) {
        switch ( level ) {
            case Level::Trace:
                return { .color = TerminalColor::BrightBlack };

            case Level::Debug:
                return { .color = TerminalColor::Cyan };

            case Level::Info:
                return { .color = TerminalColor::Green };

            case Level::Warning:
                return { .color = TerminalColor::Yellow };

            case Level::Error:
                return { .color = TerminalColor::Red };

            case Level::Off:
                return { .color = TerminalColor::BrightBlack };
        }

        return { .color = TerminalColor::Magenta };
    }

} // namespace ts::log
