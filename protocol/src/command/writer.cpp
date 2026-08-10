#include <cstddef>
#include <protocol/command/writer.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    CommandWriter::CommandWriter( std::string_view name ) {
        ValidateName( name );

        m_Data = name;
    }

    void CommandWriter::Write( std::string_view name ) {
        WritePrefix( name );

        m_RowHasParameter = true;
    }

    void CommandWriter::Write( std::string_view name, std::string_view value ) {
        WritePrefix( name );

        m_Data += '=';
        m_Data += EncodeValue( value );

        m_RowHasParameter = true;
    }

    void CommandWriter::NextRow() {
        if ( !m_RowHasParameter ) {
            throw std::runtime_error( "Cannot create an empty command row" );
        }

        m_Data += '|';

        m_FirstRow = false;

        m_RowHasParameter = false;
    }

    std::vector<std::byte> CommandWriter::Take() const {
        if ( !m_FirstRow && !m_RowHasParameter ) {
            throw std::runtime_error( "Command contains a trailing empty row" );
        }

        std::vector<std::byte> result;

        result.reserve( m_Data.size() );

        for ( const char character : m_Data ) {
            result.push_back( static_cast<std::byte>( static_cast<unsigned char>( character ) ) );
        }

        return result;
    }

    void CommandWriter::ValidateName( std::string_view name ) {
        if ( name.empty() ) {
            throw std::runtime_error( "Command name cannot be empty" );
        }

        for ( const char character : name ) {
            switch ( character ) {
                case '\0':
                case ' ':
                case '=':
                case '|':
                    throw std::runtime_error( "Invalid command name" );

                default:
                    break;
            }
        }
    }

    std::string CommandWriter::EncodeValue( std::string_view value ) {
        std::string result;

        result.reserve( value.size() );

        for ( const char character : value ) {
            switch ( character ) {
                case '\\':
                    result += "\\\\";
                    break;

                case '/':
                    result += "\\/";
                    break;

                case ' ':
                    result += "\\s";
                    break;

                case '|':
                    result += "\\p";
                    break;

                case '\a':
                    result += "\\a";
                    break;

                case '\b':
                    result += "\\b";
                    break;

                case '\f':
                    result += "\\f";
                    break;

                case '\n':
                    result += "\\n";
                    break;

                case '\r':
                    result += "\\r";
                    break;

                case '\t':
                    result += "\\t";
                    break;

                case '\v':
                    result += "\\v";
                    break;

                case '\0':
                    throw std::runtime_error( "TeamSpeak command value contains a null byte" );

                default:
                    result += character;
                    break;
            }
        }

        return result;
    }

    void CommandWriter::WritePrefix( std::string_view name ) {
        ValidateName( name );

        if ( m_FirstRow || m_RowHasParameter ) {
            m_Data += ' ';
        }

        m_Data += name;
    }

} // namespace ts::protocol
