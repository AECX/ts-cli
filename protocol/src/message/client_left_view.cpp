#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/client_left_view.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    ClientLeftView ClientLeftView::Parse( const Command& command ) {
        if ( command.Name() != "notifyclientleftview" ) {
            throw std::runtime_error( "Expected notifyclientleftview command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "notifyclientleftview contains no clients" );
        }

        ClientLeftView result;

        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            ClientLeftViewEntry entry;

            const std::uint64_t clientId = ParseUnsigned( row.Require( "clid" ), "clid" );

            if ( clientId == 0 || clientId > std::numeric_limits<std::uint16_t>::max() ) {
                throw std::runtime_error( "Invalid notifyclientleftview clid" );
            }

            entry.id = static_cast<std::uint16_t>( clientId );

            if ( const auto fromChannelId = row.Find( "cfid" ) ) {
                entry.fromChannelId = ParseUnsigned( *fromChannelId, "cfid" );
            }

            if ( const auto toChannelId = row.Find( "ctid" ) ) {
                entry.toChannelId = ParseUnsigned( *toChannelId, "ctid" );
            }

            if ( const auto reasonId = row.Find( "reasonid" ) ) {
                entry.reasonId = ParseUnsigned( *reasonId, "reasonid" );
            }

            if ( const auto reasonMessage = row.Find( "reasonmsg" ) ) {
                entry.reasonMessage = *reasonMessage;
            }

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<ClientLeftViewEntry>& ClientLeftView::Entries() const {
        return m_Entries;
    }

    std::uint64_t ClientLeftView::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
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
