#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <protocol/command/parser.hpp>
#include <protocol/message/channel_list.hpp>
#include <protocol/message/client_enter_view.hpp>
#include <protocol/message/client_left_view.hpp>
#include <protocol/message/client_moved.hpp>
#include <protocol/message/client_updated.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <stdexcept>
#include <string>

namespace ts::test {

    void RunClientStateTests() {
        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifycliententerview "
                                                                              "cfid=0 "
                                                                              "ctid=7 "
                                                                              "reasonid=2 "
                                                                              "client_unique_identifier=abc123= "
                                                                              "client_nickname=aecx "
                                                                              "client_input_muted=1 "
                                                                              "client_output_muted=0 "
                                                                              "client_input_hardware=0 "
                                                                              "client_output_hardware=1 "
                                                                              "client_is_recording=1 "
                                                                              "client_is_priority_speaker=1 "
                                                                              "client_is_channel_commander=1 "
                                                                              "client_away=0 "
                                                                              "client_type=0 "
                                                                              "clid=13" );

            const protocol::ClientEnterView message = protocol::ClientEnterView::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 1 }, "ClientEnterView parsed the wrong number of clients" );

            const auto& client = message.Entries().front();

            ExpectEqual( client.id, std::uint16_t { 13 }, "Client ID was parsed incorrectly" );
            ExpectEqual( client.channelId, std::uint64_t { 7 }, "Client channel ID was parsed incorrectly" );
            ExpectEqual( client.nickname, std::string( "aecx" ), "Client nickname was parsed incorrectly" );
            ExpectEqual( client.uniqueId, std::string( "abc123=" ), "Client unique ID was parsed incorrectly" );
            Expect( client.inputMuted, "Client input mute was parsed incorrectly" );
            Expect( !client.outputMuted, "Client output mute was parsed incorrectly" );
            Expect( !client.away, "Client away state was parsed incorrectly" );
            Expect( !client.inputHardware, "Client input hardware state was parsed incorrectly" );
            Expect( client.outputHardware, "Client output hardware state was parsed incorrectly" );
            Expect( client.recording, "Client recording state was parsed incorrectly" );
            Expect( client.prioritySpeaker, "Client priority-speaker state was parsed incorrectly" );
            Expect( client.channelCommander, "Client channel-commander state was parsed incorrectly" );
            Expect( !client.serverQuery, "Normal client was incorrectly marked as ServerQuery" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifycliententerview "
                                                                              "ctid=1 "
                                                                              "client_nickname=Alice "
                                                                              "client_type=0 "
                                                                              "clid=2"
                                                                              "|"
                                                                              "ctid=1 "
                                                                              "client_nickname=Bob "
                                                                              "client_type=0 "
                                                                              "clid=3" );

            const protocol::ClientEnterView message = protocol::ClientEnterView::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 2 }, "Multi-row ClientEnterView was parsed incorrectly" );
        }

        {
            protocol::ChannelStore channels;

            const protocol::Command channelCommand = protocol::CommandParser::Parse( "channellist "
                                                                                     "cid=1 "
                                                                                     "cpid=0 "
                                                                                     "channel_name=Lounge "
                                                                                     "channel_order=0"
                                                                                     "|"
                                                                                     "cid=7 "
                                                                                     "cpid=0 "
                                                                                     "channel_name=Admin "
                                                                                     "channel_order=1" );

            channels.Apply( protocol::ChannelList::Parse( channelCommand ) );
            channels.Validate();

            protocol::ClientStore clients;

            const protocol::Command clientCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                    "ctid=7 "
                                                                                    "client_nickname=Charlie "
                                                                                    "clid=10"
                                                                                    "|"
                                                                                    "ctid=1 "
                                                                                    "client_nickname=Bob "
                                                                                    "clid=11"
                                                                                    "|"
                                                                                    "ctid=1 "
                                                                                    "client_nickname=Alice "
                                                                                    "clid=12" );

            clients.Apply( protocol::ClientEnterView::Parse( clientCommand ) );
            clients.Validate( channels );

            ExpectEqual( clients.Size(), std::size_t { 3 }, "ClientStore contains the wrong number of clients" );

            const protocol::Client* bob = clients.Find( 11 );

            Expect( bob != nullptr, "ClientStore could not find an existing client" );
            ExpectEqual( bob->nickname, std::string( "Bob" ), "ClientStore returned the wrong client" );
            Expect( bob->detailsKnown, "Entered client was not marked as complete" );

            const auto loungeClients = clients.InChannel( 1 );

            ExpectEqual( loungeClients.size(), std::size_t { 2 }, "Wrong number of clients returned for channel" );
            ExpectEqual( loungeClients[0]->nickname, std::string( "Alice" ), "Channel clients were not sorted by nickname" );
            ExpectEqual( loungeClients[1]->nickname, std::string( "Bob" ), "Channel clients were not sorted by nickname" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command first = protocol::CommandParser::Parse( "notifycliententerview "
                                                                            "ctid=1 "
                                                                            "client_nickname=OldName "
                                                                            "clid=5" );

            clients.Apply( protocol::ClientEnterView::Parse( first ) );

            const protocol::Command second = protocol::CommandParser::Parse( "notifycliententerview "
                                                                             "ctid=2 "
                                                                             "client_nickname=NewName "
                                                                             "clid=5" );

            clients.Apply( protocol::ClientEnterView::Parse( second ) );

            ExpectEqual( clients.Size(), std::size_t { 1 }, "Repeated client entry created a duplicate client" );

            const protocol::Client* client = clients.Find( 5 );

            Expect( client != nullptr, "Updated client disappeared from ClientStore" );
            ExpectEqual( client->channelId, std::uint64_t { 2 }, "Repeated client entry did not update channel" );
            ExpectEqual( client->nickname, std::string( "NewName" ), "Repeated client entry did not update nickname" );
        }

        {
            protocol::ChannelStore channels;

            const protocol::Command channelCommand = protocol::CommandParser::Parse( "channellist "
                                                                                     "cid=1 "
                                                                                     "cpid=0 "
                                                                                     "channel_name=Lounge "
                                                                                     "channel_order=0" );

            channels.Apply( protocol::ChannelList::Parse( channelCommand ) );

            protocol::ClientStore clients;

            const protocol::Command clientCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                    "ctid=999 "
                                                                                    "client_nickname=Lost "
                                                                                    "clid=5" );

            clients.Apply( protocol::ClientEnterView::Parse( clientCommand ) );

            ExpectThrows<std::runtime_error>(
                [&clients, &channels]() {
                    clients.Validate( channels );
                },
                "Client referencing unknown channel was accepted" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifycliententerview "
                                                                              "clid=42 "
                                                                              "client_nickname=Unplaced "
                                                                              "client_unique_identifier=abc123 "
                                                                              "client_type=0" );

            const protocol::ClientEnterView message = protocol::ClientEnterView::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 1 }, "Client without ctid was not parsed" );
            ExpectEqual( message.Entries()[0].id, std::uint16_t { 42 }, "Unplaced client ID was parsed incorrectly" );
            ExpectEqual( message.Entries()[0].channelId,
                         std::uint64_t { 0 },
                         "Missing ctid should produce an unresolved channel" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifyclientmoved "
                                                                              "ctid=7 "
                                                                              "reasonid=0 "
                                                                              "clid=13" );

            const protocol::ClientMoved message = protocol::ClientMoved::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 1 }, "ClientMoved parsed the wrong number of clients" );
            ExpectEqual( message.Entries()[0].id, std::uint16_t { 13 }, "ClientMoved parsed the wrong client ID" );
            ExpectEqual( message.Entries()[0].channelId, std::uint64_t { 7 }, "ClientMoved parsed the wrong channel ID" );
            ExpectEqual( message.Entries()[0].reasonId, std::uint64_t { 0 }, "ClientMoved parsed the wrong reason ID" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command enterCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                   "ctid=1 "
                                                                                   "client_nickname=Alice "
                                                                                   "clid=2" );

            clients.Apply( protocol::ClientEnterView::Parse( enterCommand ) );

            const protocol::Command moveCommand = protocol::CommandParser::Parse( "notifyclientmoved "
                                                                                  "ctid=7 "
                                                                                  "reasonid=0 "
                                                                                  "clid=2" );

            clients.Apply( protocol::ClientMoved::Parse( moveCommand ) );

            const protocol::Client* alice = clients.Find( 2 );

            Expect( alice != nullptr, "Moved client disappeared from ClientStore" );
            ExpectEqual( alice->channelId, std::uint64_t { 7 }, "Client move did not update the channel" );
            ExpectEqual( alice->nickname, std::string( "Alice" ), "Client move discarded client details" );
            Expect( clients.InChannel( 1 ).empty(), "Moved client remained in the old channel" );
            ExpectEqual( clients.InChannel( 7 ).size(), std::size_t { 1 }, "Moved client was not added to the new channel" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command moveCommand = protocol::CommandParser::Parse( "notifyclientmoved "
                                                                                  "ctid=7 "
                                                                                  "reasonid=0 "
                                                                                  "clid=42" );

            clients.Apply( protocol::ClientMoved::Parse( moveCommand ) );

            const protocol::Client* partial = clients.Find( 42 );

            Expect( partial != nullptr, "Unknown moved client was discarded" );
            ExpectEqual( partial->channelId, std::uint64_t { 7 }, "Unknown moved client lost channel placement" );
            Expect( !partial->detailsKnown, "Unknown moved client was incorrectly marked as complete" );

            const protocol::Command enterCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                   "client_nickname=LateArrival "
                                                                                   "clid=42" );

            clients.Apply( protocol::ClientEnterView::Parse( enterCommand ) );

            const protocol::Client* completed = clients.Find( 42 );

            Expect( completed != nullptr, "Completed client disappeared from ClientStore" );
            ExpectEqual( completed->channelId,
                         std::uint64_t { 7 },
                         "Enter notification without ctid discarded previously known channel placement" );
            ExpectEqual( completed->nickname, std::string( "LateArrival" ), "Late client details were not applied" );
            Expect( completed->detailsKnown, "Late client details were not marked complete" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifyclientleftview "
                                                                              "reasonmsg=disconnected "
                                                                              "reasonid=8 "
                                                                              "clid=2 "
                                                                              "cfid=7 "
                                                                              "ctid=0" );

            const protocol::ClientLeftView message = protocol::ClientLeftView::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 1 }, "ClientLeftView parsed the wrong number of clients" );
            ExpectEqual( message.Entries()[0].id, std::uint16_t { 2 }, "ClientLeftView parsed the wrong client ID" );
            ExpectEqual( message.Entries()[0].fromChannelId,
                         std::uint64_t { 7 },
                         "ClientLeftView parsed the wrong source channel" );
            ExpectEqual( message.Entries()[0].toChannelId,
                         std::uint64_t { 0 },
                         "ClientLeftView parsed the wrong target channel" );
            ExpectEqual( message.Entries()[0].reasonId, std::uint64_t { 8 }, "ClientLeftView parsed the wrong reason ID" );
            ExpectEqual( message.Entries()[0].reasonMessage,
                         std::string( "disconnected" ),
                         "ClientLeftView parsed the wrong reason message" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command enterCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                   "ctid=7 "
                                                                                   "client_nickname=Alice "
                                                                                   "clid=2" );

            clients.Apply( protocol::ClientEnterView::Parse( enterCommand ) );

            const protocol::Command leftCommand = protocol::CommandParser::Parse( "notifyclientleftview "
                                                                                  "reasonid=8 "
                                                                                  "clid=2 "
                                                                                  "cfid=7 "
                                                                                  "ctid=0" );

            clients.Apply( protocol::ClientLeftView::Parse( leftCommand ) );

            Expect( clients.Find( 2 ) == nullptr, "ClientLeftView did not remove the client" );
            ExpectEqual( clients.Size(), std::size_t { 0 }, "ClientStore size was not updated after client left" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command enterCommand = protocol::CommandParser::Parse( "notifycliententerview "
                                                                                   "ctid=7 "
                                                                                   "client_nickname=OldName "
                                                                                   "clid=12" );

            clients.Apply( protocol::ClientEnterView::Parse( enterCommand ) );

            const protocol::Command updateCommand = protocol::CommandParser::Parse( "notifyclientupdated "
                                                                                    "clid=12 "
                                                                                    "client_nickname=NewName "
                                                                                    "client_away=1 "
                                                                                    "client_input_muted=1 "
                                                                                    "client_output_hardware=0" );

            clients.Apply( protocol::ClientUpdated::Parse( updateCommand ) );

            const protocol::Client* client = clients.Find( 12 );

            Expect( client != nullptr, "Updated client disappeared from ClientStore" );
            ExpectEqual( client->nickname, std::string( "NewName" ), "ClientUpdated did not update nickname" );
            ExpectEqual( client->channelId, std::uint64_t { 7 }, "ClientUpdated discarded channel placement" );
            Expect( client->away, "ClientUpdated did not update AFK state" );
            Expect( client->inputMuted, "ClientUpdated did not update input mute state" );
            Expect( !client->outputHardware, "ClientUpdated did not update output hardware state" );
        }

        {
            protocol::ClientStore clients;
            const protocol::Command moveCommand =
                protocol::CommandParser::Parse( "notifyclientmoved ctid=7 reasonid=0 clid=55" );
            clients.Apply( protocol::ClientMoved::Parse( moveCommand ) );

            const protocol::Command staleEnter = protocol::CommandParser::Parse(
                "notifycliententerview ctid=3 client_unique_identifier=stable-uid client_nickname=MovedFirst clid=55" );
            clients.Apply( protocol::ClientEnterView::Parse( staleEnter ) );

            const protocol::Client* client = clients.Find( 55 );
            Expect( client != nullptr && client->detailsKnown, "Move-before-enter client was not completed" );
            ExpectEqual( client->channelId,
                         std::uint64_t { 7 },
                         "Stale enter snapshot overwrote a newer move-before-enter placement" );
            ExpectEqual( client->uniqueId, std::string( "stable-uid" ), "Move-before-enter lost remote identity" );
        }

        {
            protocol::ClientStore clients;
            const protocol::Command enter = protocol::CommandParser::Parse(
                "notifycliententerview ctid=9 client_nickname=One clid=1|ctid=9 client_nickname=Two clid=2|"
                "ctid=8 client_nickname=Keep clid=3" );
            clients.Apply( protocol::ClientEnterView::Parse( enter ) );
            clients.RemoveInChannel( 9, 2 );
            Expect( clients.Find( 1 ) == nullptr, "RemoveInChannel kept an ordinary client" );
            Expect( clients.Find( 2 ) != nullptr, "RemoveInChannel removed the preserved client" );
            Expect( clients.Find( 3 ) != nullptr, "RemoveInChannel affected another channel" );
        }

        {
            protocol::ClientStore clients;

            const protocol::Command command = protocol::CommandParser::Parse( "notifycliententerview "
                                                                              "ctid=1 "
                                                                              "client_nickname=Later "
                                                                              "clid=12"
                                                                              "|"
                                                                              "ctid=1 "
                                                                              "client_nickname=Earlier "
                                                                              "clid=4" );

            clients.Apply( protocol::ClientEnterView::Parse( command ) );

            const auto all = clients.All();

            ExpectEqual( all.size(), std::size_t { 2 }, "ClientStore::All returned the wrong number of clients" );
            ExpectEqual( all[0]->id, std::uint16_t { 4 }, "ClientStore::All did not preserve client ID order" );
            ExpectEqual( all[1]->id, std::uint16_t { 12 }, "ClientStore::All did not preserve client ID order" );
        }
    }

} // namespace ts::test
