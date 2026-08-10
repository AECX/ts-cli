#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/command_result.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    CommandResult CommandResult::Parse( const Command& command ) {
        if ( command.Name() != "error" ) {
            throw std::runtime_error( "Expected error command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "error command contains no results" );
        }

        CommandResult result;
        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            const std::uint64_t parsedId = ParseUnsigned( row.Require( "id" ), "id" );

            if ( parsedId > std::numeric_limits<std::uint32_t>::max() ) {
                throw std::runtime_error( "Invalid error command id" );
            }

            CommandResultEntry entry { .id = static_cast<std::uint32_t>( parsedId ),
                                       .message = std::string( row.Require( "msg" ) ),
                                       .returnCode = std::nullopt };

            if ( const auto returnCode = row.Find( "return_code" ) ) {
                entry.returnCode = *returnCode;
            }

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<CommandResultEntry>& CommandResult::Entries() const {
        return m_Entries;
    }

    std::uint64_t CommandResult::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
        if ( value.empty() ) {
            throw std::runtime_error( "Empty unsigned command parameter: " + std::string( parameterName ) );
        }

        std::uint64_t result = 0;

        const char* first = value.data();
        const char* last = first + value.size();

        const auto parsed = std::from_chars( first, last, result );

        if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
            throw std::runtime_error( "Invalid unsigned command parameter: " + std::string( parameterName ) );
        }

        return result;
    }

} // namespace ts::protocol
