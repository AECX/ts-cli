#ifndef TS_PROTOCOL_COMMAND_COMMAND_HPP
#define TS_PROTOCOL_COMMAND_COMMAND_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::protocol {

    class CommandParser;

    class CommandParameter {
      public:
        CommandParameter( std::string name, std::string value ): m_Name( std::move( name ) ), m_Value( std::move( value ) ) {
        }

        [[nodiscard]] const std::string& Name() const {
            return m_Name;
        }

        [[nodiscard]] const std::string& Value() const {
            return m_Value;
        }

      private:
        std::string m_Name;
        std::string m_Value;
    };

    class CommandRow {
      public:
        [[nodiscard]] const std::vector<CommandParameter>& Parameters() const {
            return m_Parameters;
        }

        [[nodiscard]] std::optional<std::string_view> Find( std::string_view name ) const {
            for ( const CommandParameter& parameter : m_Parameters ) {
                if ( parameter.Name() == name ) {
                    return parameter.Value();
                }
            }

            return std::nullopt;
        }

        [[nodiscard]] std::string_view Require( std::string_view name ) const {
            const auto value = Find( name );

            if ( !value ) {
                throw std::runtime_error( "Missing command parameter: " + std::string( name ) );
            }

            return *value;
        }

      private:
        explicit CommandRow( std::vector<CommandParameter> parameters ): m_Parameters( std::move( parameters ) ) {
        }

        std::vector<CommandParameter> m_Parameters;

        friend class CommandParser;
    };

    class Command {
      public:
        [[nodiscard]] const std::string& Name() const {
            return m_Name;
        }

        [[nodiscard]] const std::vector<CommandRow>& Rows() const {
            return m_Rows;
        }

      private:
        Command( std::string name, std::vector<CommandRow> rows ): m_Name( std::move( name ) ), m_Rows( std::move( rows ) ) {
        }

        std::string m_Name;
        std::vector<CommandRow> m_Rows;

        friend class CommandParser;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_COMMAND_COMMAND_HPP
