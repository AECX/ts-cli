#include "test_support.hpp"

#include <cstddef>
#include <protocol/command/parser.hpp>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ts::test {

    void RunCommandParserTests() {
        {
            const protocol::Command command = protocol::CommandParser::Parse( "channellist "
                                                                              "cid=1 "
                                                                              "channel_name=Main\\sLobby"
                                                                              "|"
                                                                              "cid=2 "
                                                                              "channel_name=Gaming\\pAFK" );

            ExpectEqual( command.Name(), std::string_view( "channellist" ), "Command name was parsed incorrectly" );

            ExpectEqual( command.Rows().size(), std::size_t { 2 }, "Command row count is incorrect" );

            ExpectEqual( command.Rows()[0].Require( "cid" ), std::string_view( "1" ), "First row cid is incorrect" );

            ExpectEqual( command.Rows()[0].Require( "channel_name" ),
                         std::string_view( "Main Lobby" ),
                         "Space escape was decoded incorrectly" );

            ExpectEqual( command.Rows()[1].Require( "cid" ), std::string_view( "2" ), "Second row cid is incorrect" );

            ExpectEqual( command.Rows()[1].Require( "channel_name" ),
                         std::string_view( "Gaming|AFK" ),
                         "Pipe escape was decoded incorrectly" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "clientinit "
                                                                              "client_default_channel "
                                                                              "client_server_password" );

            ExpectEqual( command.Rows().size(), std::size_t { 1 }, "Bare-parameter command should have one row" );

            const auto defaultChannel = command.Rows()[0].Find( "client_default_channel" );

            Expect( defaultChannel.has_value(), "Bare parameter was not preserved" );

            Expect( defaultChannel->empty(), "Bare parameter should have an empty value" );

            const auto password = command.Rows()[0].Find( "client_server_password" );

            Expect( password.has_value(), "Second bare parameter was not preserved" );

            Expect( password->empty(), "Second bare parameter should have an empty value" );
        }

        {
            const auto data = Bytes( "notifytextmessage "
                                     "targetmode=1 "
                                     "msg=hello\\sworld\r\n" );

            const protocol::Command command = protocol::CommandParser::Parse( std::span<const std::byte>( data ) );

            ExpectEqual( command.Name(),
                         std::string_view( "notifytextmessage" ),
                         "Byte-span command name was parsed incorrectly" );

            ExpectEqual( command.Rows()[0].Require( "targetmode" ),
                         std::string_view( "1" ),
                         "Byte-span parameter was parsed incorrectly" );

            ExpectEqual( command.Rows()[0].Require( "msg" ),
                         std::string_view( "hello world" ),
                         "Byte-span escaped value was parsed incorrectly" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "test value="
                                                                              "\\\\"
                                                                              "\\/"
                                                                              "\\s"
                                                                              "\\p"
                                                                              "\\a"
                                                                              "\\b"
                                                                              "\\f"
                                                                              "\\n"
                                                                              "\\r"
                                                                              "\\t"
                                                                              "\\v" );

            const std::string_view expected = "\\/ |\a\b\f\n\r\t\v";

            ExpectEqual( command.Rows()[0].Require( "value" ), expected, "Command escape decoding is incorrect" );
        }

        ExpectThrows<std::runtime_error>(
            []() {
                (void)protocol::CommandParser::Parse( "" );
            },
            "Empty command did not fail" );

        ExpectThrows<std::runtime_error>(
            []() {
                (void)protocol::CommandParser::Parse( "test value=hello\\" );
            },
            "Incomplete escape did not fail" );

        ExpectThrows<std::runtime_error>(
            []() {
                (void)protocol::CommandParser::Parse( "test value=hello\\x" );
            },
            "Unknown escape did not fail" );

        ExpectThrows<std::runtime_error>(
            []() {
                (void)protocol::CommandParser::Parse( "test a=1||b=2" );
            },
            "Empty command row did not fail" );
    }

} // namespace ts::test
