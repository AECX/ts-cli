#include "test_support.hpp"

#include <cstdint>
#include <protocol/command/parser.hpp>
#include <protocol/message/client_poke.hpp>
#include <stdexcept>
#include <string>

namespace ts::test {

    void RunClientPokeTests() {
        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifyclientpoke "
                                                                              "invokerid=11 "
                                                                              "invokername=User "
                                                                              "invokeruid=abc123= "
                                                                              "msg=hey\\sthere" );

            const protocol::ClientPoke poke = protocol::ClientPoke::Parse( command );
            const protocol::ClientPokeEntry& entry = poke.Entry();

            ExpectEqual( entry.invokerId, std::uint16_t { 11 }, "Poke invoker ID was parsed incorrectly" );
            ExpectEqual( entry.invokerName, std::string( "User" ), "Poke invoker name was parsed incorrectly" );
            ExpectEqual( entry.invokerUniqueId, std::string( "abc123=" ), "Poke invoker UID was parsed incorrectly" );
            ExpectEqual( entry.message, std::string( "hey there" ), "Poke escaping was not decoded by the command parser" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifyclientpoke "
                                                                              "invokername=User "
                                                                              "invokeruid=abc123= "
                                                                              "msg=hi" );

            ExpectThrows<std::runtime_error>(
                [&command]() {
                    (void)protocol::ClientPoke::Parse( command );
                },
                "Missing invokerid was not rejected" );
        }

        {
            const protocol::Command command = protocol::CommandParser::Parse( "notifytextmessage targetmode=2 msg=x" );

            ExpectThrows<std::runtime_error>(
                [&command]() {
                    (void)protocol::ClientPoke::Parse( command );
                },
                "Wrong command name was not rejected" );
        }
    }

} // namespace ts::test
