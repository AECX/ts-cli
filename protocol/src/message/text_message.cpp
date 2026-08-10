#include <charconv>
#include <cstdint>
#include <limits>
#include <protocol/message/text_message.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ts::protocol {

    TextMessage TextMessage::Parse( const Command& command ) {
        if ( command.Name() != "notifytextmessage" ) {
            throw std::runtime_error( "Expected notifytextmessage command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "notifytextmessage contains no messages" );
        }

        TextMessage result;
        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            TextMessageEntry entry;

            entry.targetMode = ParseTargetMode( row.Require( "targetmode" ) );
            entry.text = row.Require( "msg" );

            if ( const auto target = row.Find( "target" ) ) {
                entry.targetId = ParseUnsigned( *target, "target" );
            }

            if ( const auto invokerId = row.Find( "invokerid" ) ) {
                const std::uint64_t parsedInvokerId = ParseUnsigned( *invokerId, "invokerid" );

                if ( parsedInvokerId > std::numeric_limits<std::uint16_t>::max() ) {
                    throw std::runtime_error( "Invalid notifytextmessage invokerid" );
                }

                entry.invokerId = static_cast<std::uint16_t>( parsedInvokerId );
            }

            if ( const auto invokerName = row.Find( "invokername" ) ) {
                entry.invokerName = *invokerName;
            }

            if ( const auto invokerUniqueId = row.Find( "invokeruid" ) ) {
                entry.invokerUniqueId = *invokerUniqueId;
            }

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<TextMessageEntry>& TextMessage::Entries() const {
        return m_Entries;
    }

    std::uint64_t TextMessage::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
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

    TextMessageTargetMode TextMessage::ParseTargetMode( std::string_view value ) {
        const std::uint64_t targetMode = ParseUnsigned( value, "targetmode" );

        switch ( targetMode ) {
            case 1:
                return TextMessageTargetMode::Private;

            case 2:
                return TextMessageTargetMode::Channel;

            case 3:
                return TextMessageTargetMode::Server;

            default:
                throw std::runtime_error( "Invalid notifytextmessage targetmode" );
        }
    }

} // namespace ts::protocol
