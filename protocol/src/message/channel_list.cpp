#include <charconv>
#include <cstdint>
#include <protocol/message/channel_list.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace ts::protocol {

    ChannelList ChannelList::Parse( const Command& command ) {
        if ( command.Name() != "channellist" ) {
            throw std::runtime_error( "Expected channellist command" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "channellist contains no channels" );
        }

        ChannelList result;

        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            ChannelListEntry entry;

            entry.id = ParseUnsigned( row.Require( "cid" ), "cid" );

            if ( entry.id == 0 ) {
                throw std::runtime_error( "Invalid channellist cid" );
            }

            entry.parentId = ParseUnsigned( row.Require( "cpid" ), "cpid" );

            entry.orderAfterId = ParseUnsigned( row.Require( "channel_order" ), "channel_order" );

            entry.name = row.Require( "channel_name" );

            entry.permanent = ParseOptionalBoolean( row, "channel_flag_permanent" );

            entry.semiPermanent = ParseOptionalBoolean( row, "channel_flag_semi_permanent" );

            entry.defaultChannel = ParseOptionalBoolean( row, "channel_flag_default" );

            entry.passwordProtected = ParseOptionalBoolean( row, "channel_flag_password" );

            if ( const auto codec = row.Find( "channel_codec" ) ) {
                const std::uint64_t parsedCodec = ParseUnsigned( *codec, "channel_codec" );

                if ( parsedCodec > 5 ) {
                    throw std::runtime_error( "Invalid channel_codec" );
                }

                entry.codec = static_cast<std::uint8_t>( parsedCodec );
            }

            if ( row.Find( "channel_codec_is_unencrypted" ) ) {
                entry.codecIsUnencrypted = ParseOptionalBoolean( row, "channel_codec_is_unencrypted" );
            }

            result.m_Entries.push_back( std::move( entry ) );
        }

        return result;
    }

    const std::vector<ChannelListEntry>& ChannelList::Entries() const {
        return m_Entries;
    }

    std::uint64_t ChannelList::ParseUnsigned( std::string_view value, std::string_view parameterName ) {
        std::uint64_t result = 0;

        const char* first = value.data();

        const char* last = first + value.size();

        const auto parseResult = std::from_chars( first, last, result );

        if ( parseResult.ec != std::errc {} || parseResult.ptr != last ) {
            throw std::runtime_error( "Invalid unsigned command parameter: " + std::string( parameterName ) );
        }

        return result;
    }

    bool ChannelList::ParseOptionalBoolean( const CommandRow& row, std::string_view parameterName ) {
        const auto value = row.Find( parameterName );

        if ( !value ) {
            return false;
        }

        const std::uint64_t parsed = ParseUnsigned( *value, parameterName );

        if ( parsed > 1 ) {
            throw std::runtime_error( "Invalid boolean command parameter: " + std::string( parameterName ) );
        }

        return parsed != 0;
    }

} // namespace ts::protocol
