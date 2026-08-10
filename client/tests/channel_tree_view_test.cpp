#include "test_support.hpp"

#include <algorithm>
#include <client/cli/channel_tree_view.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <protocol/command/parser.hpp>
#include <protocol/message/channel_list.hpp>
#include <protocol/message/client_enter_view.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <string>
#include <vector>

namespace ts::client::test {

    namespace {

        protocol::ChannelStore BuildChannels() {
            protocol::ChannelStore channels;

            const protocol::Command command = protocol::CommandParser::Parse( "channellist "
                                                                              "cid=1 "
                                                                              "cpid=0 "
                                                                              "channel_name=Lobby "
                                                                              "channel_order=0 "
                                                                              "channel_flag_default=1 "
                                                                              "channel_flag_password=0"
                                                                              "|"
                                                                              "cid=2 "
                                                                              "cpid=0 "
                                                                              "channel_name=Gaming "
                                                                              "channel_order=1 "
                                                                              "channel_flag_default=0 "
                                                                              "channel_flag_password=1"
                                                                              "|"
                                                                              "cid=3 "
                                                                              "cpid=2 "
                                                                              "channel_name=Raids "
                                                                              "channel_order=0" );

            channels.Apply( protocol::ChannelList::Parse( command ) );
            return channels;
        }

        protocol::ClientStore BuildClients() {
            protocol::ClientStore clients;

            const protocol::Command command = protocol::CommandParser::Parse( "notifycliententerview "
                                                                              "ctid=1 "
                                                                              "client_nickname=Alice "
                                                                              "client_type=0 "
                                                                              "clid=101"
                                                                              "|"
                                                                              "ctid=2 "
                                                                              "client_nickname=Bob "
                                                                              "client_away=1 "
                                                                              "client_type=0 "
                                                                              "clid=102" );

            clients.Apply( protocol::ClientEnterView::Parse( command ) );

            /* A move that arrived before its enter-view leaves a partial, "loading" client. */
            clients.Move( 103, 3 );

            return clients;
        }

    } // namespace

    void RunChannelTreeViewTests() {
        const protocol::ChannelStore channels = BuildChannels();
        const protocol::ClientStore clients = BuildClients();
        const std::vector<protocol::ChannelTreeEntry> tree = channels.Tree();

        {
            const cli::ChannelTreeRange range { .begin = 0, .end = tree.size(), .baseDepth = 0, .rootIsUnadorned = false };
            const std::vector<std::string> lines = cli::FormatChannelTree( tree, range, clients, 999 );

            ExpectEqual( lines.size(), std::size_t { 6 }, "Full tree rendered the wrong number of lines" );
            ExpectEqual( lines[0], std::string( "├── [1] Lobby [default]" ), "Root channel was not rendered as a branch" );
            ExpectEqual( lines[1], std::string( "│   └── Alice [101]" ), "Lone client under a non-last channel lost its bar" );
            ExpectEqual( lines[2],
                         std::string( "└── [2] Gaming [password]" ),
                         "Last root channel was not rendered as an elbow" );
            ExpectEqual( lines[3],
                         std::string( "    ├── Bob [102] [afk]" ),
                         "Client sharing its channel with a subchannel was rendered as last" );
            ExpectEqual( lines[4], std::string( "    └── [3] Raids" ), "Nested channel lost its ancestor indentation" );
            ExpectEqual( lines[5],
                         std::string( "        └── client 103 [103] [loading]" ),
                         "Partial client was not rendered as loading" );
        }

        {
            const auto rootEntry = std::find_if( tree.begin(), tree.end(), []( const protocol::ChannelTreeEntry& entry ) {
                return entry.channel->id == 2;
            } );
            Expect( rootEntry != tree.end(), "Test fixture is missing the Gaming channel" );

            const std::size_t begin = static_cast<std::size_t>( std::distance( tree.begin(), rootEntry ) );
            const cli::ChannelTreeRange range { .begin = begin,
                                                .end = tree.size(),
                                                .baseDepth = rootEntry->depth,
                                                .rootIsUnadorned = true };
            const std::vector<std::string> lines = cli::FormatChannelTree( tree, range, clients, 999 );

            ExpectEqual( lines.size(), std::size_t { 4 }, "Subtree view rendered the wrong number of lines" );
            ExpectEqual( lines[0], std::string( "[2] Gaming [password]" ), "Subtree root should be printed bare" );
            ExpectEqual( lines[1], std::string( "    ├── Bob [102] [afk]" ), "Subtree client lost its connector" );
            ExpectEqual( lines[2], std::string( "    └── [3] Raids" ), "Subtree channel lost its connector" );
            ExpectEqual( lines[3],
                         std::string( "        └── client 103 [103] [loading]" ),
                         "Subtree's nested client was not rendered as loading" );
        }
    }

} // namespace ts::client::test
