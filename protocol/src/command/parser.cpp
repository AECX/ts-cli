#include <protocol/command/parser.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    Command CommandParser::Parse( std::span<const std::byte> data ) {
        if ( data.empty() ) {
            throw std::runtime_error( "Cannot parse empty command" );
        }

        const std::string_view text( reinterpret_cast<const char*>( data.data() ), data.size() );

        return Parse( text );
    }

    Command CommandParser::Parse( std::string_view text ) {
        while ( !text.empty() && ( text.back() == '\r' || text.back() == '\n' ) ) {
            text.remove_suffix( 1 );
        }

        if ( text.empty() ) {
            throw std::runtime_error( "Cannot parse empty command" );
        }

        const std::size_t nameEnd = text.find( ' ' );

        std::string name;

        if ( nameEnd == std::string_view::npos ) {
            name = std::string( text );

            return Command( std::move( name ), {} );
        }

        name = std::string( text.substr( 0, nameEnd ) );

        if ( name.empty() ) {
            throw std::runtime_error( "Command has no name" );
        }

        std::size_t dataStart = nameEnd;

        while ( dataStart < text.size() && text[dataStart] == ' ' ) {
            ++dataStart;
        }

        if ( dataStart == text.size() ) {
            return Command( std::move( name ), {} );
        }

        const std::string_view data = text.substr( dataStart );

        std::vector<CommandRow> rows;

        std::size_t rowStart = 0;

        while ( rowStart <= data.size() ) {
            const std::size_t rowEnd = data.find( '|', rowStart );

            const std::size_t length = rowEnd == std::string_view::npos ? data.size() - rowStart : rowEnd - rowStart;

            const std::string_view row = data.substr( rowStart, length );

            if ( row.empty() ) {
                throw std::runtime_error( "Command contains empty row" );
            }

            rows.push_back( ParseRow( row ) );

            if ( rowEnd == std::string_view::npos ) {
                break;
            }

            rowStart = rowEnd + 1;
        }

        return Command( std::move( name ), std::move( rows ) );
    }

    CommandRow CommandParser::ParseRow( std::string_view text ) {
        std::vector<CommandParameter> parameters;

        std::size_t offset = 0;

        while ( offset < text.size() ) {
            while ( offset < text.size() && text[offset] == ' ' ) {
                ++offset;
            }

            if ( offset == text.size() ) {
                break;
            }

            const std::size_t parameterEnd = text.find( ' ', offset );

            const std::size_t length = parameterEnd == std::string_view::npos ? text.size() - offset : parameterEnd - offset;

            const std::string_view parameter = text.substr( offset, length );

            const std::size_t separator = parameter.find( '=' );

            if ( separator == std::string_view::npos ) {
                parameters.emplace_back( std::string( parameter ), std::string {} );
            } else {
                const std::string_view name = parameter.substr( 0, separator );

                const std::string_view value = parameter.substr( separator + 1 );

                if ( name.empty() ) {
                    throw std::runtime_error( "Command parameter has no name" );
                }

                parameters.emplace_back( std::string( name ), DecodeValue( value ) );
            }

            if ( parameterEnd == std::string_view::npos ) {
                break;
            }

            offset = parameterEnd + 1;
        }

        return CommandRow( std::move( parameters ) );
    }

    std::string CommandParser::DecodeValue( std::string_view value ) {
        std::string result;

        result.reserve( value.size() );

        for ( std::size_t i = 0; i < value.size(); ++i ) {
            if ( value[i] != '\\' ) {
                result.push_back( value[i] );

                continue;
            }

            if ( i + 1 >= value.size() ) {
                throw std::runtime_error( "Incomplete command escape sequence" );
            }

            ++i;

            switch ( value[i] ) {
                case '\\':
                    result.push_back( '\\' );
                    break;

                case '/':
                    result.push_back( '/' );
                    break;

                case 's':
                    result.push_back( ' ' );
                    break;

                case 'p':
                    result.push_back( '|' );
                    break;

                case 'a':
                    result.push_back( '\a' );
                    break;

                case 'b':
                    result.push_back( '\b' );
                    break;

                case 'f':
                    result.push_back( '\f' );
                    break;

                case 'n':
                    result.push_back( '\n' );
                    break;

                case 'r':
                    result.push_back( '\r' );
                    break;

                case 't':
                    result.push_back( '\t' );
                    break;

                case 'v':
                    result.push_back( '\v' );
                    break;

                default:
                    throw std::runtime_error( "Unknown command escape sequence" );
            }
        }

        return result;
    }

} // namespace ts::protocol
