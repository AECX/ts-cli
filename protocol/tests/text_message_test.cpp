#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <protocol/command/parser.hpp>
#include <protocol/message/text_message.hpp>
#include <stdexcept>
#include <string>

namespace ts::test {

    void RunTextMessageTests() {
        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage "
                                                                              "targetmode=2 "
                                                                              "msg=hey\\sbuddy "
                                                                              "invokerid=11 "
                                                                              "invokername=User "
                                                                              "invokeruid=abc123=" );

            const protocol::TextMessage message = protocol::TextMessage::Parse( command );

            ExpectEqual( message.Entries().size(), std::size_t { 1 }, "TextMessage parsed the wrong number of messages" );

            const protocol::TextMessageEntry& entry = message.Entries().front();

            Expect( entry.targetMode == protocol::TextMessageTargetMode::Channel,
                    "Channel text message target mode was parsed incorrectly" );
            Expect( !entry.targetId.has_value(), "Missing text message target should stay absent" );
            ExpectEqual( entry.invokerId, std::uint16_t { 11 }, "Text message invoker ID was parsed incorrectly" );
            ExpectEqual( entry.invokerName, std::string( "User" ), "Text message invoker name was parsed incorrectly" );
            ExpectEqual( entry.invokerUniqueId, std::string( "abc123=" ), "Text message invoker UID was parsed incorrectly" );
            ExpectEqual( entry.text,
                         std::string( "hey buddy" ),
                         "Text message escaping was not decoded by the command parser" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage "
                                                                              "targetmode=1 "
                                                                              "target=14 "
                                                                              "msg=hello "
                                                                              "invokerid=7 "
                                                                              "invokername=Alice" );

            const protocol::TextMessage message = protocol::TextMessage::Parse( command );
            const protocol::TextMessageEntry& entry = message.Entries().front();

            Expect( entry.targetMode == protocol::TextMessageTargetMode::Private,
                    "Private text message target mode was parsed incorrectly" );
            Expect( entry.targetId.has_value(), "Explicit private text message target was discarded" );
            ExpectEqual( *entry.targetId, std::uint64_t { 14 }, "Private text message target was parsed incorrectly" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage "
                                                                              "targetmode=3 "
                                                                              "msg=server\\snotice" );

            const protocol::TextMessage message = protocol::TextMessage::Parse( command );
            const protocol::TextMessageEntry& entry = message.Entries().front();

            Expect( entry.targetMode == protocol::TextMessageTargetMode::Server,
                    "Server text message target mode was parsed incorrectly" );
            ExpectEqual( entry.invokerId, std::uint16_t { 0 }, "Missing server-message invoker ID should remain zero" );
            ExpectEqual( entry.text, std::string( "server notice" ), "Server text message was parsed incorrectly" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage targetmode=9 msg=invalid" );

            ExpectThrows<std::runtime_error>(
                [&command]() {
                    static_cast<void>( protocol::TextMessage::Parse( command ) );
                },
                "Invalid text message target mode was accepted" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage targetmode=2 invokerid=11" );

            ExpectThrows<std::runtime_error>(
                [&command]() {
                    static_cast<void>( protocol::TextMessage::Parse( command ) );
                },
                "Text message without msg was accepted" );
        }
    }

} // namespace ts::test
