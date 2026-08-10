#ifndef TS_LOG_LOGGER_HPP
#define TS_LOG_LOGGER_HPP

#include "level.hpp"
#include "sink.hpp"

#include <string_view>

namespace ts::log {

    class Logger {
      public:
        explicit Logger( Sink& sink, Level minimumLevel = Level::Info );

        void SetLevel( Level minimumLevel );

        [[nodiscard]] Level MinimumLevel() const;

        void Write( Level level, std::string_view component, std::string_view message ) const;

        void Trace( std::string_view component, std::string_view message ) const;

        void Debug( std::string_view component, std::string_view message ) const;

        void Info( std::string_view component, std::string_view message ) const;

        void Warning( std::string_view component, std::string_view message ) const;

        void Error( std::string_view component, std::string_view message ) const;

      private:
        [[nodiscard]] bool ShouldWrite( Level level ) const;

        Sink& m_Sink;
        Level m_MinimumLevel;
    };

} // namespace ts::log

#endif // TS_LOG_LOGGER_HPP
