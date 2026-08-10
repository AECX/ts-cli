#include <charconv>
#include <cstdint>
#include <protocol/message/channel_moved.hpp>
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
    } // namespace

    ChannelMoved ChannelMoved::Parse( const Command& command ) {
        if ( command.Name() != "notifychannelmoved" || command.Rows().size() != 1 ) {
            throw std::runtime_error( "Expected single notifychannelmoved command" );
        }
        const CommandRow& row = command.Rows().front();
        ChannelMoved result;
        result.m_Entry.id = ParseUnsigned( row.Require( "cid" ), "cid" );
        result.m_Entry.parentId = ParseUnsigned( row.Require( "cpid" ), "cpid" );
        result.m_Entry.orderAfterId = ParseUnsigned( row.Require( "order" ), "order" );
        if ( result.m_Entry.id == 0 ) {
            throw std::runtime_error( "Invalid notifychannelmoved cid" );
        }
        return result;
    }

    const ChannelMovedEntry& ChannelMoved::Entry() const {
        return m_Entry;
    }

} // namespace ts::protocol
