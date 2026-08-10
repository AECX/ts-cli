#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/client_enter_view.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    ClientEnterView ClientEnterView::Parse( const Command& command ) {
        if ( command.Name() != "notifycliententerview" ) {
            throw std::runtime_error( "Expected notifycliententerview command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "notifycliententerview contains no clients" );
        }

        ClientEnterView result;

        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            ClientEnterViewEntry entry;

            const std::uint64_t clientId = ParseUnsigned( row.Require( "clid" ), "clid" );

            if ( clientId == 0 || clientId > std::numeric_limits<std::uint16_t>::max() ) {
                throw std::runtime_error( "Invalid notifycliententerview clid" );
            }

            entry.id = static_cast<std::uint16_t>( clientId );

            /*
             * ctid is normally present and identifies the
             * channel the client entered.
             *
             * Do not make it mandatory, though. Depending on
             * visibility/subscription state the server can
             * provide a client notification without every
             * property being available to us.
             *
             * channelId == 0 therefore means:
             *
             *   current channel not known yet
             *
             * A later client movement/visibility notification
             * can resolve it.
             */
            if ( const auto channelId = row.Find( "ctid" ) ) {
                entry.channelId = ParseUnsigned( *channelId, "ctid" );
            }

            entry.nickname = row.Require( "client_nickname" );

            if ( entry.nickname.empty() ) {
                throw std::runtime_error( "notifycliententerview contains an empty nickname" );
            }

            if ( const auto uniqueId = row.Find( "client_unique_identifier" ) ) {
                entry.uniqueId = *uniqueId;
            }

            entry.away = ParseOptionalBoolean( row, "client_away" );

            entry.inputMuted = ParseOptionalBoolean( row, "client_input_muted" );

            entry.outputMuted = ParseOptionalBoolean( row, "client_output_muted" );

            entry.inputHardware = ParseOptionalBoolean( row, "client_input_hardware", true );
            entry.outputHardware = ParseOptionalBoolean( row, "client_output_hardware", true );
            entry.recording = ParseOptionalBoolean( row, "client_is_recording" );
            entry.prioritySpeaker = ParseOptionalBoolean( row, "client_is_priority_speaker" );
            entry.channelCommander = ParseOptionalBoolean( row, "client_is_channel_commander" );

            entry.serverQuery = ParseOptionalBoolean( row, "client_type" );

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<ClientEnterViewEntry>& ClientEnterView::Entries() const {
        return m_Entries;
    }

    std::uint64_t ClientEnterView::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
        if ( value.empty() ) {
            throw std::runtime_error( "Empty unsigned command parameter: " + std::string( parameterName ) );
        }

        std::uint64_t result = 0;

        const char* first = value.data();

        const char* last = first + value.size();

        const auto parseResult = std::from_chars( first, last, result );

        if ( parseResult.ec != std::errc {} || parseResult.ptr != last ) {
            throw std::runtime_error( "Invalid unsigned command parameter: " + std::string( parameterName ) );
        }

        return result;
    }

    bool ClientEnterView::ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName, bool defaultValue ) {
        const auto value = row.Find( parameterName );

        if ( !value ) {
            return defaultValue;
        }

        const std::uint64_t parsed = ParseUnsigned( *value, parameterName );

        if ( parsed > 1 ) {
            throw std::runtime_error( "Invalid boolean command parameter: " + std::string( parameterName ) );
        }

        return parsed != 0;
    }

} // namespace ts::protocol
