#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/client_updated.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    ClientUpdated ClientUpdated::Parse( const Command& command ) {
        if ( command.Name() != "notifyclientupdated" ) {
            throw std::runtime_error( "Expected notifyclientupdated command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "notifyclientupdated contains no clients" );
        }

        ClientUpdated result;
        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            const std::uint64_t parsedId = ParseUnsigned( row.Require( "clid" ), "clid" );

            if ( parsedId > std::numeric_limits<std::uint16_t>::max() ) {
                throw std::runtime_error( "Invalid notifyclientupdated clid" );
            }

            ClientUpdatedEntry entry;
            entry.id = static_cast<std::uint16_t>( parsedId );

            if ( const auto nickname = row.Find( "client_nickname" ) ) {
                entry.nickname = *nickname;
            }

            entry.away = ParseOptionalBoolean( row, "client_away" );
            entry.inputMuted = ParseOptionalBoolean( row, "client_input_muted" );
            entry.outputMuted = ParseOptionalBoolean( row, "client_output_muted" );
            entry.inputHardware = ParseOptionalBoolean( row, "client_input_hardware" );
            entry.outputHardware = ParseOptionalBoolean( row, "client_output_hardware" );
            entry.recording = ParseOptionalBoolean( row, "client_is_recording" );
            entry.prioritySpeaker = ParseOptionalBoolean( row, "client_is_priority_speaker" );
            entry.channelCommander = ParseOptionalBoolean( row, "client_is_channel_commander" );

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<ClientUpdatedEntry>& ClientUpdated::Entries() const {
        return m_Entries;
    }

    std::optional<bool> ClientUpdated::ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName ) {
        const auto value = row.Find( parameterName );

        if ( !value ) {
            return std::nullopt;
        }

        const std::uint64_t parsed = ParseUnsigned( *value, parameterName );

        if ( parsed > 1 ) {
            throw std::runtime_error( "Invalid boolean command parameter: " + std::string( parameterName ) );
        }

        return parsed != 0;
    }

    std::uint64_t ClientUpdated::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
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
