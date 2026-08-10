#include "test_support.hpp"

#include <chrono>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <string>
#include <variant>

namespace ts::client::test {

    void RunEventQueueTests() {
        using namespace std::chrono_literals;

        EventQueue queue;

        protocol::SessionEvent sent = protocol::TextMessageEvent {
            .message = protocol::TextMessageEntry { .targetMode = protocol::TextMessageTargetMode::Channel,
                                                    .targetId = std::nullopt,
                                                    .invokerId = 11,
                                                    .invokerName = "User",
                                                    .invokerUniqueId = "uid",
                                                    .text = "hello" },
            .replyTarget = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Channel, .id = 1 },
            .channelName = "Lobby",
            .privatePeerName = {},
            .outgoing = false };

        queue.Push( sent );

        RuntimeEvent received;

        Expect( queue.WaitPop( received, 0ms ), "Event queue did not return a queued event" );

        const auto* protocolEvent = std::get_if<ProtocolEvent>( &received );

        Expect( protocolEvent != nullptr, "Event queue did not wrap the protocol event" );

        const auto* textMessage = std::get_if<protocol::TextMessageEvent>( &protocolEvent->event );

        Expect( textMessage != nullptr, "Event queue changed the session event type" );

        ExpectEqual( textMessage->message.invokerName, std::string( "User" ), "Event queue changed the event sender" );

        Expect( !queue.WaitPop( received, 0ms ), "Empty event queue unexpectedly returned an event" );

        queue.Close();

        Expect( queue.Closed(), "Event queue did not report its closed state" );
    }

} // namespace ts::client::test
