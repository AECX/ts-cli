#include "test_support.hpp"

#include <audio/audio_types.hpp>
#include <client/gtk/conversation_log.hpp>
#include <client/gtk/event_router.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/server_snapshot.hpp>
#include <cstddef>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/session/disconnect.hpp>
#include <protocol/session/event.hpp>
#include <string>

namespace ts::client::gtk::test {

    namespace {

        [[nodiscard]] RuntimeEvent TextMessage( protocol::TextMessageTargetMode mode,
                                                std::optional<std::uint64_t> targetId,
                                                std::uint16_t invokerId,
                                                std::optional<protocol::TextMessageTarget> replyTarget,
                                                bool outgoing ) {
            return ProtocolEvent {
                .event = protocol::TextMessageEvent { .message = protocol::TextMessageEntry { .targetMode = mode,
                                                                                              .targetId = targetId,
                                                                                              .invokerId = invokerId,
                                                                                              .invokerName = "Sender",
                                                                                              .invokerUniqueId = "uid",
                                                                                              .text = "hi" },
                                                      .replyTarget = replyTarget,
                                                      .channelName = {},
                                                      .privatePeerName = {},
                                                      .outgoing = outgoing } };
        }

        [[nodiscard]] RoutedLine OnlyLine( const RoutedEvent& routed, std::string_view context ) {
            ExpectEqual( routed.lines.size(), std::size_t { 1 }, std::string( context ) + ": expected exactly one line" );
            return routed.lines.front();
        }

    } // namespace

    void RunEventRouterTests() {
        ServerSnapshot snapshot;
        snapshot.clientId = 3;
        snapshot.currentChannelId = 9;

        const RoutedEvent channel =
            RouteEvent( TextMessage( protocol::TextMessageTargetMode::Channel,
                                     7,
                                     12,
                                     protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Channel, .id = 7 },
                                     false ),
                        snapshot );
        const RoutedLine channelLine = OnlyLine( channel, "channel message" );
        Expect( channelLine.conversation == ChannelConversation( 7 ), "Channel message not routed to its channel" );
        ExpectEqual( channelLine.line.kind, FeedLineKind::Message, "Channel message is not a message line" );
        ExpectEqual( channelLine.line.sender, std::string( "Sender" ), "Channel message sender changed" );
        ExpectEqual( channelLine.line.text, std::string( "hi" ), "Channel message text changed" );
        Expect( channel.notification == audio::NotificationType::Message, "Incoming message did not notify" );

        const RoutedLine fallback = OnlyLine(
            RouteEvent( TextMessage( protocol::TextMessageTargetMode::Channel, std::nullopt, 12, std::nullopt, false ),
                        snapshot ),
            "channel fallback" );
        Expect( fallback.conversation == ChannelConversation( 9 ),
                "Unresolved channel message did not fall back to the current channel" );

        const RoutedLine incomingPrivate = OnlyLine(
            RouteEvent( TextMessage( protocol::TextMessageTargetMode::Private,
                                     3,
                                     12,
                                     protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = 12 },
                                     false ),
                        snapshot ),
            "incoming private" );
        Expect( incomingPrivate.conversation == ClientConversation( 12 ),
                "Incoming private message not routed to the sender's conversation" );

        const RoutedEvent outgoingPrivate =
            RouteEvent( TextMessage( protocol::TextMessageTargetMode::Private, 12, 3, std::nullopt, true ), snapshot );
        Expect( OnlyLine( outgoingPrivate, "outgoing private" ).conversation == ClientConversation( 12 ),
                "Outgoing private message not routed to the recipient's conversation" );
        Expect( !outgoingPrivate.notification, "Own message triggered a notification" );

        const RoutedLine server =
            OnlyLine( RouteEvent( TextMessage( protocol::TextMessageTargetMode::Server, std::nullopt, 12, std::nullopt, false ),
                                  snapshot ),
                      "server message" );
        Expect( server.conversation == ServerConversation(), "Server message not routed to the server conversation" );

        const RoutedLine presence = OnlyLine(
            RouteEvent( ProtocolEvent { .event = protocol::ClientPresenceEvent { .kind = protocol::ClientPresenceKind::Left,
                                                                                 .clientId = 12,
                                                                                 .channelId = 7,
                                                                                 .clientName = "Sender",
                                                                                 .channelName = "Lobby" } },
                        snapshot ),
            "presence" );
        Expect( presence.conversation == ChannelConversation( 7 ), "Presence not routed to its channel" );
        ExpectEqual( presence.line.text, std::string( "Sender left the channel" ), "Presence text changed" );

        const RoutedEvent error = RouteEvent( ActionErrorEvent { .message = "nope" }, snapshot );
        const RoutedLine errorLine = OnlyLine( error, "action error" );
        Expect( !errorLine.conversation, "Local error was not routed to the active conversation" );
        ExpectEqual( errorLine.line.kind, FeedLineKind::Error, "Local error is not an error line" );
        Expect( error.notification == audio::NotificationType::Failure, "Local error did not notify" );

        const RoutedLine disconnected = OnlyLine(
            RouteEvent( DisconnectedEvent { .info = protocol::DisconnectInfo { .reason = protocol::DisconnectReason::Timeout,
                                                                               .message = {} } },
                        snapshot ),
            "disconnect" );
        ExpectEqual( disconnected.line.text, std::string( "connection timed out" ), "Disconnect reason text changed" );

        Expect( RouteEvent( ServerSnapshotEvent { .snapshot = snapshot }, snapshot ).lines.empty(),
                "Snapshot event produced feed lines" );
    }

} // namespace ts::client::gtk::test
