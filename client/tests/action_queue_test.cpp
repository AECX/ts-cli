#include "test_support.hpp"

#include <client/runtime/action.hpp>
#include <client/runtime/action_queue.hpp>
#include <string>
#include <variant>

namespace ts::client::test {

    void RunActionQueueTests() {
        ActionQueue queue;

        queue.Push( SendPrivateMessageAction { .target = "User", .text = "hello" } );

        ClientAction action;

        Expect( queue.TryPop( action ), "Action queue did not return a queued action" );

        const auto* message = std::get_if<SendPrivateMessageAction>( &action );

        Expect( message != nullptr, "Action queue changed the action type" );

        ExpectEqual( message->target, std::string( "User" ), "Action queue changed the target" );

        ExpectEqual( message->text, std::string( "hello" ), "Action queue changed the message" );

        Expect( !queue.TryPop( action ), "Empty action queue unexpectedly returned an action" );

        queue.Close();

        Expect( queue.Closed(), "Action queue did not report its closed state" );
    }

} // namespace ts::client::test
