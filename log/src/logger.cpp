#include <log/logger.hpp>

namespace ts::log {

    Logger::Logger( Sink& sink, Level minimumLevel ): m_Sink( sink ), m_MinimumLevel( minimumLevel ) {
    }

    void Logger::SetLevel( Level minimumLevel ) {
        m_MinimumLevel = minimumLevel;
    }

    Level Logger::MinimumLevel() const {
        return m_MinimumLevel;
    }

    void Logger::Write( Level level, std::string_view component, std::string_view message ) const {
        if ( !ShouldWrite( level ) ) {
            return;
        }

        m_Sink.Write( Record { .level = level, .component = component, .message = message } );
    }

    void Logger::Trace( std::string_view component, std::string_view message ) const {
        Write( Level::Trace, component, message );
    }

    void Logger::Debug( std::string_view component, std::string_view message ) const {
        Write( Level::Debug, component, message );
    }

    void Logger::Info( std::string_view component, std::string_view message ) const {
        Write( Level::Info, component, message );
    }

    void Logger::Warning( std::string_view component, std::string_view message ) const {
        Write( Level::Warning, component, message );
    }

    void Logger::Error( std::string_view component, std::string_view message ) const {
        Write( Level::Error, component, message );
    }

    bool Logger::ShouldWrite( Level level ) const {
        if ( m_MinimumLevel == Level::Off ) {
            return false;
        }

        return static_cast<std::uint8_t>( level ) >= static_cast<std::uint8_t>( m_MinimumLevel );
    }

} // namespace ts::log
