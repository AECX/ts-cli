#include "test_support.hpp"

#include <cstddef>
#include <protocol/command/parser.hpp>
#include <protocol/message/client_disconnect.hpp>
#include <string>
#include <vector>

namespace ts::test {

    void RunClientDisconnectTests() {
        const protocol::ClientDisconnect disconnect( "Leaving now" );

        const std::vector<std::byte> serialized = disconnect.Serialize();
        const protocol::Command command = protocol::CommandParser::Parse( serialized );

        ExpectEqual( command.Name(), std::string( "clientdisconnect" ), "ClientDisconnect wrote the wrong command name" );

        ExpectEqual( command.Rows().size(), std::size_t { 1 }, "ClientDisconnect wrote the wrong number of rows" );

        const protocol::CommandRow& row = command.Rows().front();

        ExpectEqual( std::string( row.Require( "reasonid" ) ),
                     std::string( "8" ),
                     "ClientDisconnect wrote the wrong reason ID" );

        ExpectEqual( std::string( row.Require( "reasonmsg" ) ),
                     std::string( "Leaving now" ),
                     "ClientDisconnect wrote the wrong reason message" );
    }

} // namespace ts::test
