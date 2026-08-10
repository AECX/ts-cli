#include <charconv>
#include <cstdint>
#include <protocol/command/writer.hpp>
#include <protocol/message/channel_subscription.hpp>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

namespace ts::protocol {

    namespace {

        [[nodiscard]] std::uint64_t ParseChannelId( std::string_view value ) {
            std::uint64_t result = 0;
            const char* first = value.data();
            const char* last = first + value.size();
            const auto parsed = std::from_chars( first, last, result );

            if ( parsed.ec != std::errc {} || parsed.ptr != last || result == 0 ) {
                throw std::runtime_error( "Invalid channel subscription cid" );
            }

            return result;
        }

    } // namespace

    std::vector<std::byte> ChannelSubscribeAll::Serialize() {
        return CommandWriter( "channelsubscribeall" ).Take();
    }

    ChannelSubscribe::ChannelSubscribe( std::uint64_t channelId ): m_ChannelId( channelId ) {
        if ( m_ChannelId == 0 ) {
            throw std::runtime_error( "Cannot subscribe to channel zero" );
        }
    }

    std::vector<std::byte> ChannelSubscribe::Serialize() const {
        CommandWriter writer( "channelsubscribe" );
        writer.Write( "cid", m_ChannelId );
        return writer.Take();
    }

    ChannelSubscriptionState ChannelSubscriptionState::Parse( const Command& command ) {
        const bool subscribed = command.Name() == "notifychannelsubscribed";

        if ( !subscribed && command.Name() != "notifychannelunsubscribed" ) {
            throw std::runtime_error( "Expected channel subscription notification" );
        }

        if ( command.Rows().empty() ) {
            throw std::runtime_error( "Channel subscription notification contains no channels" );
        }

        ChannelSubscriptionState result;
        result.m_Subscribed = subscribed;
        result.m_Entries.reserve( command.Rows().size() );

        for ( const CommandRow& row : command.Rows() ) {
            result.m_Entries.push_back( ChannelSubscriptionEntry { .channelId = ParseChannelId( row.Require( "cid" ) ) } );
        }

        return result;
    }

    bool ChannelSubscriptionState::Subscribed() const {
        return m_Subscribed;
    }

    const std::vector<ChannelSubscriptionEntry>& ChannelSubscriptionState::Entries() const {
        return m_Entries;
    }

} // namespace ts::protocol
