#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/client_moved.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    ClientMoved ClientMoved::Parse( const Command& command ) {
        if ( command.Name() != "notifyclientmoved" ) {
            throw std::runtime_error( "Expected notifyclientmoved command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "notifyclientmoved contains no clients" );
        }

        ClientMoved result;

        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            ClientMovedEntry entry;

            const std::uint64_t clientId = ParseUnsigned( row.Require( "clid" ), "clid" );

            if ( clientId == 0 || clientId > std::numeric_limits<std::uint16_t>::max() ) {
                throw std::runtime_error( "Invalid notifyclientmoved clid" );
            }

            entry.id = static_cast<std::uint16_t>( clientId );
            entry.channelId = ParseUnsigned( row.Require( "ctid" ), "ctid" );
            entry.reasonId = ParseUnsigned( row.Require( "reasonid" ), "reasonid" );

            if ( const auto reasonMessage = row.Find( "reasonmsg" ) ) {
                entry.reasonMessage = *reasonMessage;
            }

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<ClientMovedEntry>& ClientMoved::Entries() const {
        return m_Entries;
    }

    std::uint64_t ClientMoved::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
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
