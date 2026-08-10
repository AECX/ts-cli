#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <protocol/state/channel_store.hpp>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    void ChannelStore::Clear() {
        m_Channels.clear();
        m_SubscribedChannels.clear();
    }

    void ChannelStore::Apply( const ChannelList& channelList ) {
        for ( const ChannelListEntry& entry : channelList.Entries() ) {
            Upsert( Channel { .id = entry.id,
                              .parentId = entry.parentId,
                              .orderAfterId = entry.orderAfterId,
                              .name = entry.name,
                              .permanent = entry.permanent,
                              .semiPermanent = entry.semiPermanent,
                              .defaultChannel = entry.defaultChannel,
                              .passwordProtected = entry.passwordProtected,
                              .codec = entry.codec,
                              .codecIsUnencrypted = entry.codecIsUnencrypted } );
        }
    }

    void ChannelStore::Apply( const ChannelCreated& created ) {
        const ChannelCreatedEntry& entry = created.Entry();

        if ( !m_Channels.contains( entry.id ) ) {
            for ( auto& [id, channel] : m_Channels ) {
                if ( id != entry.id && channel.parentId == entry.parentId && channel.orderAfterId == entry.orderAfterId ) {
                    channel.orderAfterId = entry.id;
                    break;
                }
            }
        }

        Upsert( Channel { .id = entry.id,
                          .parentId = entry.parentId,
                          .orderAfterId = entry.orderAfterId,
                          .name = entry.name,
                          .permanent = entry.permanent,
                          .semiPermanent = entry.semiPermanent,
                          .defaultChannel = entry.defaultChannel,
                          .passwordProtected = entry.passwordProtected,
                          .codec = entry.codec,
                          .codecIsUnencrypted = entry.codecIsUnencrypted } );
    }

    void ChannelStore::Apply( const ChannelEdited& edited ) {
        const ChannelEditedEntry& entry = edited.Entry();
        auto channel = m_Channels.find( entry.id );
        if ( channel == m_Channels.end() ) {
            return;
        }

        if ( entry.parentId || entry.orderAfterId ) {
            Move( entry.id,
                  entry.parentId.value_or( channel->second.parentId ),
                  entry.orderAfterId.value_or( channel->second.orderAfterId ) );
            channel = m_Channels.find( entry.id );
        }
        if ( entry.name ) {
            channel->second.name = *entry.name;
        }
        if ( entry.permanent ) {
            channel->second.permanent = *entry.permanent;
        }
        if ( entry.semiPermanent ) {
            channel->second.semiPermanent = *entry.semiPermanent;
        }
        if ( entry.defaultChannel ) {
            channel->second.defaultChannel = *entry.defaultChannel;
        }
        if ( entry.passwordProtected ) {
            channel->second.passwordProtected = *entry.passwordProtected;
        }
        if ( entry.codec ) {
            channel->second.codec = *entry.codec;
        }
        if ( entry.codecIsUnencrypted ) {
            channel->second.codecIsUnencrypted = *entry.codecIsUnencrypted;
        }
    }

    void ChannelStore::Apply( const ChannelMoved& moved ) {
        const ChannelMovedEntry& entry = moved.Entry();
        Move( entry.id, entry.parentId, entry.orderAfterId );
    }

    void ChannelStore::Apply( const ChannelSubscriptionState& subscription ) {
        for ( const ChannelSubscriptionEntry& entry : subscription.Entries() ) {
            SetSubscribed( entry.channelId, subscription.Subscribed() );
        }
    }

    void ChannelStore::SetSubscribed( std::uint64_t channelId, bool subscribed ) {
        if ( channelId == 0 ) {
            return;
        }

        if ( subscribed ) {
            m_SubscribedChannels.insert( channelId );
        } else {
            m_SubscribedChannels.erase( channelId );
        }

        const auto channel = m_Channels.find( channelId );
        if ( channel != m_Channels.end() ) {
            channel->second.subscribed = subscribed;
        }
    }

    void ChannelStore::Remove( std::uint64_t channelId ) {
        const auto removed = m_Channels.find( channelId );
        if ( removed == m_Channels.end() ) {
            m_SubscribedChannels.erase( channelId );
            return;
        }

        const std::uint64_t parentId = removed->second.parentId;
        const std::uint64_t predecessorId = removed->second.orderAfterId;

        for ( auto& [id, channel] : m_Channels ) {
            (void)id;
            if ( channel.parentId == parentId && channel.orderAfterId == channelId ) {
                channel.orderAfterId = predecessorId;
            }
        }

        m_Channels.erase( removed );
        m_SubscribedChannels.erase( channelId );
    }

    void ChannelStore::Upsert( Channel channel ) {
        channel.subscribed = m_SubscribedChannels.contains( channel.id );
        m_Channels.insert_or_assign( channel.id, std::move( channel ) );
    }

    void ChannelStore::Move( std::uint64_t channelId, std::uint64_t parentId, std::uint64_t orderAfterId ) {
        auto moved = m_Channels.find( channelId );
        if ( moved == m_Channels.end() ) {
            return;
        }
        if ( orderAfterId == channelId ) {
            throw std::runtime_error( "Channel cannot be ordered after itself" );
        }
        if ( parentId != 0 && !m_Channels.contains( parentId ) ) {
            throw std::runtime_error( "Channel move references an unknown parent" );
        }
        if ( orderAfterId != 0 ) {
            const auto predecessor = m_Channels.find( orderAfterId );
            if ( predecessor == m_Channels.end() || predecessor->second.parentId != parentId ) {
                throw std::runtime_error( "Channel move references an invalid predecessor" );
            }
        }

        const std::uint64_t oldParentId = moved->second.parentId;
        const std::uint64_t oldPredecessorId = moved->second.orderAfterId;

        for ( auto& [id, channel] : m_Channels ) {
            if ( id != channelId && channel.parentId == oldParentId && channel.orderAfterId == channelId ) {
                channel.orderAfterId = oldPredecessorId;
                break;
            }
        }

        for ( auto& [id, channel] : m_Channels ) {
            if ( id != channelId && channel.parentId == parentId && channel.orderAfterId == orderAfterId ) {
                channel.orderAfterId = channelId;
                break;
            }
        }

        moved->second.parentId = parentId;
        moved->second.orderAfterId = orderAfterId;
    }

    void ChannelStore::Validate() const {
        (void)Tree();
    }

    const Channel* ChannelStore::Find( std::uint64_t channelId ) const {
        const auto channel = m_Channels.find( channelId );

        if ( channel == m_Channels.end() ) {
            return nullptr;
        }

        return &channel->second;
    }

    bool ChannelStore::IsSubscribed( std::uint64_t channelId ) const {
        return m_SubscribedChannels.contains( channelId );
    }

    std::size_t ChannelStore::Size() const {
        return m_Channels.size();
    }

    std::vector<ChannelTreeEntry> ChannelStore::Tree() const {
        /*
         * Parent references are only validated after
         * channellistfinished. While channellist packets are
         * arriving, a child may legitimately arrive before
         * its parent.
         */
        for ( const auto& [id, channel] : m_Channels ) {
            (void)id;

            if ( channel.parentId != 0 && !m_Channels.contains( channel.parentId ) ) {
                throw std::runtime_error( "Channel references an unknown parent" );
            }
        }

        std::vector<ChannelTreeEntry> result;

        result.reserve( m_Channels.size() );

        std::set<std::uint64_t> visited;

        AppendTree( 0, 0, visited, result );

        /*
         * Any remaining channels are disconnected from the
         * root hierarchy. This normally means a parent cycle.
         */
        if ( result.size() != m_Channels.size() ) {
            throw std::runtime_error( "Channel hierarchy contains a cycle" );
        }

        return result;
    }

    std::vector<const Channel*> ChannelStore::OrderedChildren( std::uint64_t parentId ) const {
        std::vector<const Channel*> children;

        for ( const auto& [id, channel] : m_Channels ) {
            (void)id;

            if ( channel.parentId == parentId ) {
                children.push_back( &channel );
            }
        }

        if ( children.empty() ) {
            return {};
        }

        std::vector<const Channel*> ordered;

        ordered.reserve( children.size() );

        std::set<std::uint64_t> emitted;

        std::uint64_t previousId = 0;

        while ( ordered.size() < children.size() ) {
            const Channel* next = nullptr;

            for ( const Channel* candidate : children ) {
                if ( emitted.contains( candidate->id ) ) {
                    continue;
                }

                if ( candidate->orderAfterId != previousId ) {
                    continue;
                }

                if ( next != nullptr ) {
                    throw std::runtime_error( "Multiple channels occupy the same order position" );
                }

                next = candidate;
            }

            if ( next == nullptr ) {
                throw std::runtime_error( "Invalid TeamSpeak channel order chain" );
            }

            ordered.push_back( next );

            emitted.insert( next->id );

            previousId = next->id;
        }

        return ordered;
    }

    void ChannelStore::AppendTree( std::uint64_t parentId,
                                   std::size_t depth,
                                   std::set<std::uint64_t>& visited,
                                   std::vector<ChannelTreeEntry>& result ) const {
        const auto children = OrderedChildren( parentId );

        for ( const Channel* channel : children ) {
            const auto [iterator, inserted] = visited.insert( channel->id );

            (void)iterator;

            if ( !inserted ) {
                throw std::runtime_error( "Channel hierarchy contains a cycle" );
            }

            result.push_back( ChannelTreeEntry { .channel = channel,

                                                 .depth = depth } );

            AppendTree( channel->id, depth + 1, visited, result );
        }
    }

} // namespace ts::protocol
