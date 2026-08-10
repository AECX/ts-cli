#include <charconv>
#include <cstdint>
#include <protocol/message/channel_deleted.hpp>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace ts::protocol {

    ChannelDeleted ChannelDeleted::Parse( const Command& command ) {
        if ( command.Name() != "notifychanneldeleted" || command.Rows().size() != 1 ) {
            throw std::runtime_error( "Expected single notifychanneldeleted command" );
        }
        const std::string_view value = command.Rows().front().Require( "cid" );
        std::uint64_t channelId = 0;
        const char* first = value.data();
        const char* last = first + value.size();
        const auto parsed = std::from_chars( first, last, channelId );
        if ( parsed.ec != std::errc {} || parsed.ptr != last || channelId == 0 ) {
            throw std::runtime_error( "Invalid notifychanneldeleted cid" );
        }
        ChannelDeleted result;
        result.m_ChannelId = channelId;
        return result;
    }

    std::uint64_t ChannelDeleted::ChannelId() const {
        return m_ChannelId;
    }

} // namespace ts::protocol
