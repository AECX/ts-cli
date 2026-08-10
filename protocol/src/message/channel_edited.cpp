#include <charconv>
#include <cstdint>
#include <optional>
#include <protocol/message/channel_edited.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace ts::protocol {

    namespace {
        [[nodiscard]] std::uint64_t ParseUnsigned( std::string_view value, std::string_view name ) {
            std::uint64_t result = 0;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars( first, last, result );
            if ( parsed.ec != std::errc {} || parsed.ptr != last ) {
                throw std::runtime_error( "Invalid unsigned command parameter: " + std::string( name ) );
            }
            return result;
        }

        [[nodiscard]] std::optional<bool> ParseBoolean( const CommandRow& row, std::string_view name ) {
            const auto value = row.Find( name );
            if ( !value ) {
                return std::nullopt;
            }
            const std::uint64_t parsed = ParseUnsigned( *value, name );
            if ( parsed > 1 ) {
                throw std::runtime_error( "Invalid boolean command parameter: " + std::string( name ) );
            }
            return parsed != 0;
        }
    } // namespace

    ChannelEdited ChannelEdited::Parse( const Command& command ) {
        if ( command.Name() != "notifychanneledited" || command.Rows().size() != 1 ) {
            throw std::runtime_error( "Expected single notifychanneledited command" );
        }
        const CommandRow& row = command.Rows().front();
        ChannelEdited result;
        result.m_Entry.id = ParseUnsigned( row.Require( "cid" ), "cid" );
        if ( result.m_Entry.id == 0 ) {
            throw std::runtime_error( "Invalid notifychanneledited cid" );
        }
        if ( const auto value = row.Find( "cpid" ) ) {
            result.m_Entry.parentId = ParseUnsigned( *value, "cpid" );
        }
        if ( const auto value = row.Find( "channel_order" ) ) {
            result.m_Entry.orderAfterId = ParseUnsigned( *value, "channel_order" );
        }
        if ( const auto value = row.Find( "channel_name" ) ) {
            result.m_Entry.name = *value;
        }
        result.m_Entry.permanent = ParseBoolean( row, "channel_flag_permanent" );
        result.m_Entry.semiPermanent = ParseBoolean( row, "channel_flag_semi_permanent" );
        result.m_Entry.defaultChannel = ParseBoolean( row, "channel_flag_default" );
        result.m_Entry.passwordProtected = ParseBoolean( row, "channel_flag_password" );
        result.m_Entry.codecIsUnencrypted = ParseBoolean( row, "channel_codec_is_unencrypted" );
        if ( const auto value = row.Find( "channel_codec" ) ) {
            const std::uint64_t codec = ParseUnsigned( *value, "channel_codec" );
            if ( codec > 5 ) {
                throw std::runtime_error( "Invalid channel_codec" );
            }
            result.m_Entry.codec = static_cast<std::uint8_t>( codec );
        }
        return result;
    }

    const ChannelEditedEntry& ChannelEdited::Entry() const {
        return m_Entry;
    }

} // namespace ts::protocol
