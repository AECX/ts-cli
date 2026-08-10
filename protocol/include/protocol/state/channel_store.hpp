#ifndef TS_PROTOCOL_STATE_CHANNEL_STORE_HPP
#define TS_PROTOCOL_STATE_CHANNEL_STORE_HPP

#include "channel.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <protocol/message/channel_created.hpp>
#include <protocol/message/channel_edited.hpp>
#include <protocol/message/channel_list.hpp>
#include <protocol/message/channel_moved.hpp>
#include <protocol/message/channel_subscription.hpp>
#include <set>
#include <vector>

namespace ts::protocol {

    class ChannelStore {
      public:
        void Clear();

        void Apply( const ChannelList& channelList );
        void Apply( const ChannelCreated& created );
        void Apply( const ChannelEdited& edited );
        void Apply( const ChannelMoved& moved );
        void Apply( const ChannelSubscriptionState& subscription );
        void SetSubscribed( std::uint64_t channelId, bool subscribed );
        void Remove( std::uint64_t channelId );

        void Validate() const;

        [[nodiscard]] const Channel* Find( std::uint64_t channelId ) const;
        [[nodiscard]] bool IsSubscribed( std::uint64_t channelId ) const;
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::vector<ChannelTreeEntry> Tree() const;

      private:
        void Upsert( Channel channel );
        void Move( std::uint64_t channelId, std::uint64_t parentId, std::uint64_t orderAfterId );

        [[nodiscard]] std::vector<const Channel*> OrderedChildren( std::uint64_t parentId ) const;

        void AppendTree( std::uint64_t parentId,
                         std::size_t depth,
                         std::set<std::uint64_t>& visited,
                         std::vector<ChannelTreeEntry>& result ) const;

        std::map<std::uint64_t, Channel> m_Channels;
        std::set<std::uint64_t> m_SubscribedChannels;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_STATE_CHANNEL_STORE_HPP
