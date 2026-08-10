#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <protocol/command/parser.hpp>
#include <protocol/message/channel_subscription.hpp>
#include <protocol/message/client_move.hpp>
#include <protocol/message/client_update.hpp>
#include <protocol/message/client_updated.hpp>
#include <protocol/message/command_result.hpp>
#include <protocol/message/send_text_message.hpp>
#include <protocol/message/text_message.hpp>
#include <string>
#include <vector>

namespace ts::test {

    void RunClientCommandTests() {

        {
            const protocol::Command command = protocol::CommandParser::Parse( protocol::ChannelSubscribeAll::Serialize() );
            ExpectEqual( command.Name(),
                         std::string( "channelsubscribeall" ),
                         "ChannelSubscribeAll wrote the wrong command name" );
        }

        {
            const protocol::ChannelSubscribe subscribe( 42 );
            const protocol::Command command = protocol::CommandParser::Parse( subscribe.Serialize() );
            ExpectEqual( command.Name(), std::string( "channelsubscribe" ), "ChannelSubscribe wrote the wrong command" );
            ExpectEqual( std::string( command.Rows().front().Require( "cid" ) ),
                         std::string( "42" ),
                         "ChannelSubscribe wrote the wrong channel ID" );
        }

        {
            const protocol::SendTextMessage message(
                protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = 11 },
                "hey buddy" );

            const protocol::Command command = protocol::CommandParser::Parse( message.Serialize() );

            ExpectEqual( command.Name(), std::string( "sendtextmessage" ), "SendTextMessage wrote the wrong command name" );

            const protocol::CommandRow& row = command.Rows().front();

            ExpectEqual( std::string( row.Require( "targetmode" ) ),
                         std::string( "1" ),
                         "SendTextMessage wrote the wrong target mode" );

            ExpectEqual( std::string( row.Require( "target" ) ),
                         std::string( "11" ),
                         "SendTextMessage wrote the wrong target ID" );

            ExpectEqual( std::string( row.Require( "msg" ) ),
                         std::string( "hey buddy" ),
                         "SendTextMessage wrote the wrong message" );
        }

        {
            const protocol::ClientMove move( 14, 7 );
            const protocol::Command command = protocol::CommandParser::Parse( move.Serialize() );
            const protocol::CommandRow& row = command.Rows().front();

            ExpectEqual( command.Name(), std::string( "clientmove" ), "ClientMove wrote the wrong command name" );

            ExpectEqual( std::string( row.Require( "clid" ) ), std::string( "14" ), "ClientMove wrote the wrong client ID" );

            ExpectEqual( std::string( row.Require( "cid" ) ), std::string( "7" ), "ClientMove wrote the wrong channel ID" );
        }

        {
            const protocol::ClientUpdate update( "new nick" );
            const protocol::Command command = protocol::CommandParser::Parse( update.Serialize() );

            ExpectEqual( command.Name(), std::string( "clientupdate" ), "ClientUpdate wrote the wrong command name" );

            ExpectEqual( std::string( command.Rows().front().Require( "client_nickname" ) ),
                         std::string( "new nick" ),
                         "ClientUpdate wrote the wrong nickname" );
        }

        {
            const std::string input =
                "notifyclientupdated clid=14 client_nickname=new\\snick client_away=1 client_output_muted=1";

            const std::vector<std::byte> bytes( reinterpret_cast<const std::byte*>( input.data() ),
                                                reinterpret_cast<const std::byte*>( input.data() + input.size() ) );

            const protocol::Command command = protocol::CommandParser::Parse( bytes );
            const protocol::ClientUpdated updated = protocol::ClientUpdated::Parse( command );

            ExpectEqual( updated.Entries().size(), std::size_t { 1 }, "ClientUpdated parsed the wrong number of entries" );

            ExpectEqual( updated.Entries().front().id, std::uint16_t { 14 }, "ClientUpdated parsed the wrong client ID" );

            Expect( updated.Entries().front().nickname.has_value(), "ClientUpdated did not parse the nickname" );

            ExpectEqual( *updated.Entries().front().nickname,
                         std::string( "new nick" ),
                         "ClientUpdated parsed the wrong nickname" );
            Expect( updated.Entries().front().away.has_value() && *updated.Entries().front().away,
                    "ClientUpdated did not parse away state" );
            Expect( updated.Entries().front().outputMuted.has_value() && *updated.Entries().front().outputMuted,
                    "ClientUpdated did not parse output mute state" );
        }
        {
            const std::string input = "error id=2568 msg=insufficient\\sclient\\spermissions";

            const std::vector<std::byte> bytes( reinterpret_cast<const std::byte*>( input.data() ),
                                                reinterpret_cast<const std::byte*>( input.data() + input.size() ) );

            const protocol::Command command = protocol::CommandParser::Parse( bytes );
            const protocol::CommandResult result = protocol::CommandResult::Parse( command );

            ExpectEqual( result.Entries().size(), std::size_t { 1 }, "CommandResult parsed the wrong number of entries" );

            ExpectEqual( result.Entries().front().id, std::uint32_t { 2568 }, "CommandResult parsed the wrong error ID" );

            ExpectEqual( result.Entries().front().message,
                         std::string( "insufficient client permissions" ),
                         "CommandResult parsed the wrong error message" );
        }
    }

} // namespace ts::test
