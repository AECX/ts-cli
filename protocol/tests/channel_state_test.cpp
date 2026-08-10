#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <protocol/command/parser.hpp>
#include <protocol/message/channel_created.hpp>
#include <protocol/message/channel_deleted.hpp>
#include <protocol/message/channel_edited.hpp>
#include <protocol/message/channel_list.hpp>
#include <protocol/message/channel_moved.hpp>
#include <protocol/message/channel_subscription.hpp>
#include <protocol/state/channel_store.hpp>
#include <stdexcept>
#include <string>

namespace ts::test {

    void RunChannelStateTests() {
        {
            const protocol::Command command = protocol::CommandParser::Parse( "channellist "
                                                                              "cid=10 "
                                                                              "cpid=0 "
                                                                              "channel_name=Lobby "
                                                                              "channel_order=0 "
                                                                              "channel_flag_permanent=1 "
                                                                              "channel_flag_semi_permanent=0 "
                                                                              "channel_flag_default=1 "
                                                                              "channel_flag_password=0"
                                                                              "|"
                                                                              "cid=20 "
                                                                              "cpid=0 "
                                                                              "channel_name=Gaming "
                                                                              "channel_order=10 "
                                                                              "channel_flag_permanent=1 "
                                                                              "channel_flag_semi_permanent=0 "
                                                                              "channel_flag_default=0 "
                                                                              "channel_flag_password=1" );

            const protocol::ChannelList list = protocol::ChannelList::Parse( command );

            ExpectEqual( list.Entries().size(), std::size_t { 2 }, "ChannelList parsed the wrong number of channels" );

            ExpectEqual( list.Entries()[0].id, std::uint64_t { 10 }, "Channel ID was parsed incorrectly" );

            ExpectEqual( list.Entries()[0].parentId, std::uint64_t { 0 }, "Channel parent ID was parsed incorrectly" );

            ExpectEqual( list.Entries()[0].name, std::string( "Lobby" ), "Channel name was parsed incorrectly" );

            Expect( list.Entries()[0].defaultChannel, "Default channel flag was parsed incorrectly" );

            Expect( list.Entries()[1].passwordProtected, "Password channel flag was parsed incorrectly" );

            ExpectEqual( list.Entries()[1].orderAfterId,
                         std::uint64_t { 10 },
                         "Channel order predecessor was parsed incorrectly" );
        }

        {
            protocol::ChannelStore store;

            const protocol::Command firstCommand = protocol::CommandParser::Parse( "channellist "
                                                                                   "cid=10 "
                                                                                   "cpid=0 "
                                                                                   "channel_name=Lobby "
                                                                                   "channel_order=0"
                                                                                   "|"
                                                                                   "cid=20 "
                                                                                   "cpid=0 "
                                                                                   "channel_name=Gaming "
                                                                                   "channel_order=10" );

            store.Apply( protocol::ChannelList::Parse( firstCommand ) );

            /*
             * Apply a second channellist separately. Real TS3
             * servers split large snapshots over multiple
             * commands.
             */
            const protocol::Command secondCommand = protocol::CommandParser::Parse( "channellist "
                                                                                    "cid=21 "
                                                                                    "cpid=20 "
                                                                                    "channel_name=General "
                                                                                    "channel_order=0"
                                                                                    "|"
                                                                                    "cid=22 "
                                                                                    "cpid=20 "
                                                                                    "channel_name=Raids "
                                                                                    "channel_order=21" );

            store.Apply( protocol::ChannelList::Parse( secondCommand ) );

            store.Validate();

            ExpectEqual( store.Size(), std::size_t { 4 }, "ChannelStore contains the wrong number of channels" );

            const auto tree = store.Tree();

            ExpectEqual( tree.size(), std::size_t { 4 }, "Channel tree contains the wrong number of entries" );

            ExpectEqual( tree[0].channel->id, std::uint64_t { 10 }, "First root channel is ordered incorrectly" );

            ExpectEqual( tree[0].depth, std::size_t { 0 }, "Root channel has the wrong depth" );

            ExpectEqual( tree[1].channel->id, std::uint64_t { 20 }, "Second root channel is ordered incorrectly" );

            ExpectEqual( tree[2].channel->id, std::uint64_t { 21 }, "First child channel is ordered incorrectly" );

            ExpectEqual( tree[2].depth, std::size_t { 1 }, "Child channel has the wrong depth" );

            ExpectEqual( tree[3].channel->id, std::uint64_t { 22 }, "Second child channel is ordered incorrectly" );

            const protocol::Channel* gaming = store.Find( 20 );

            Expect( gaming != nullptr, "ChannelStore could not find an existing channel" );

            ExpectEqual( gaming->name, std::string( "Gaming" ), "ChannelStore returned the wrong channel" );
        }

        {
            protocol::ChannelStore store;
            const protocol::Command listCommand =
                protocol::CommandParser::Parse( "channellist cid=1 cpid=0 channel_name=Lobby channel_order=0 channel_codec=4"
                                                "|cid=2 cpid=0 channel_name=Music channel_order=1 channel_codec=5" );
            store.Apply( protocol::ChannelList::Parse( listCommand ) );

            const protocol::Command subscribedCommand = protocol::CommandParser::Parse( "notifychannelsubscribed cid=1|cid=2" );
            store.Apply( protocol::ChannelSubscriptionState::Parse( subscribedCommand ) );
            Expect( store.IsSubscribed( 1 ) && store.IsSubscribed( 2 ), "Channel subscriptions were not recorded" );
            Expect( store.Find( 1 )->subscribed && store.Find( 2 )->subscribed,
                    "Channel subscription flags were not reflected in channel state" );

            const protocol::Command unsubscribedCommand = protocol::CommandParser::Parse( "notifychannelunsubscribed cid=2" );
            store.Apply( protocol::ChannelSubscriptionState::Parse( unsubscribedCommand ) );
            Expect( store.IsSubscribed( 1 ) && !store.IsSubscribed( 2 ), "Channel unsubscription was not recorded" );
        }

        {
            protocol::ChannelStore store;
            store.Apply( protocol::ChannelList::Parse(
                protocol::CommandParser::Parse( "channellist cid=1 cpid=0 channel_name=One channel_order=0"
                                                "|cid=2 cpid=0 channel_name=Two channel_order=1" ) ) );

            store.Apply( protocol::ChannelCreated::Parse( protocol::CommandParser::Parse(
                "notifychannelcreated cid=3 cpid=0 channel_order=1 channel_name=Inserted channel_codec=4" ) ) );
            auto tree = store.Tree();
            ExpectEqual( tree.size(), std::size_t { 3 }, "Created channel was not added" );
            ExpectEqual( tree[1].channel->id, std::uint64_t { 3 }, "Created channel was inserted in the wrong order" );
            ExpectEqual( tree[2].channel->id, std::uint64_t { 2 }, "Created channel did not preserve its successor" );

            store.Apply( protocol::ChannelEdited::Parse(
                protocol::CommandParser::Parse( "notifychanneledited cid=3 channel_name=Renamed channel_codec=5" ) ) );
            ExpectEqual( store.Find( 3 )->name, std::string( "Renamed" ), "Channel edit did not update name" );
            ExpectEqual( store.Find( 3 )->codec, std::uint8_t { 5 }, "Channel edit did not update codec" );

            store.Apply(
                protocol::ChannelMoved::Parse( protocol::CommandParser::Parse( "notifychannelmoved cid=3 cpid=0 order=2" ) ) );
            tree = store.Tree();
            ExpectEqual( tree[2].channel->id, std::uint64_t { 3 }, "Channel move did not update ordering" );

            const protocol::ChannelDeleted deleted =
                protocol::ChannelDeleted::Parse( protocol::CommandParser::Parse( "notifychanneldeleted cid=3 reasonid=0" ) );
            store.Remove( deleted.ChannelId() );
            Expect( store.Find( 3 ) == nullptr, "Deleted channel remained in the store" );
            store.Validate();
        }

        {
            protocol::ChannelStore store;

            const protocol::Command command = protocol::CommandParser::Parse( "channellist "
                                                                              "cid=1 "
                                                                              "cpid=0 "
                                                                              "channel_name=Broken "
                                                                              "channel_order=999" );

            store.Apply( protocol::ChannelList::Parse( command ) );

            ExpectThrows<std::runtime_error>(
                [&store]() {
                    store.Validate();
                },
                "Broken channel order chain was accepted" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "channellist "
                                                                              "cid=1 "
                                                                              "cpid=0 "
                                                                              "channel_name=Broken "
                                                                              "channel_order=0 "
                                                                              "channel_flag_default=2" );

            ExpectThrows<std::runtime_error>(
                [&command]() {
                    (void)protocol::ChannelList::Parse( command );
                },
                "Invalid channel boolean was accepted" );
        }
    }

} // namespace ts::test
