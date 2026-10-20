#include <client/gtk/conversation_log.hpp>
#include <client/gtk/event_router.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/server_snapshot.hpp>
#include <cstdint>
#include <limits>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/session/disconnect.hpp>
#include <protocol/session/event.hpp>
#include <string>
#include <utility>
#include <variant>

namespace ts::client::gtk {

    namespace {

        [[nodiscard]] RoutedLine
            Line( std::optional<ConversationKey> conversation, FeedLineKind kind, std::string text, std::string sender = {} ) {
            return RoutedLine {
                .conversation = conversation,
                .line = FeedLine { .kind = kind, .time = {}, .sender = std::move( sender ), .text = std::move( text ) } };
        }

        [[nodiscard]] std::optional<ConversationKey> PrivatePeer( const protocol::TextMessageEvent& event ) {
            if ( !event.outgoing ) {
                return event.message.invokerId == 0 ? std::nullopt
                                                    : std::optional { ClientConversation( event.message.invokerId ) };
            }

            const std::optional<std::uint64_t> target = event.message.targetId;

            if ( !target || *target == 0 || *target > std::numeric_limits<std::uint16_t>::max() ) {
                return std::nullopt;
            }

            return ClientConversation( static_cast<std::uint16_t>( *target ) );
        }

        [[nodiscard]] RoutedEvent RouteTextMessage( const protocol::TextMessageEvent& event, const ServerSnapshot& snapshot ) {
            const protocol::TextMessageEntry& message = event.message;
            std::string sender =
                message.invokerName.empty() ? "client " + std::to_string( message.invokerId ) : message.invokerName;
            std::optional<ConversationKey> conversation;

            switch ( message.targetMode ) {
                case protocol::TextMessageTargetMode::Private:
                    conversation = PrivatePeer( event );
                    break;

                case protocol::TextMessageTargetMode::Channel:
                    conversation = ChannelConversation( event.replyTarget ? event.replyTarget->id : snapshot.currentChannelId );
                    break;

                case protocol::TextMessageTargetMode::Server:
                    conversation = ServerConversation();
                    break;
            }

            RoutedEvent routed;
            routed.lines.push_back( Line( conversation, FeedLineKind::Message, message.text, std::move( sender ) ) );

            if ( !event.outgoing ) {
                routed.notification = audio::NotificationType::Message;
            }

            return routed;
        }

        [[nodiscard]] std::string DescribeDisconnect( const protocol::DisconnectInfo& info ) {
            std::string message;

            switch ( info.reason ) {
                case protocol::DisconnectReason::ServerClosed:
                    message = "disconnected by the server";
                    break;

                case protocol::DisconnectReason::Timeout:
                    message = "connection timed out";
                    break;

                case protocol::DisconnectReason::TransportError:
                    message = "connection failed";
                    break;

                case protocol::DisconnectReason::LocalRequest:
                    message = "disconnected";
                    break;
            }

            if ( !info.message.empty() ) {
                message += ": ";
                message += info.message;
            }

            return message;
        }

        [[nodiscard]] RoutedEvent RouteSessionEvent( const protocol::SessionEvent& event, const ServerSnapshot& snapshot ) {
            RoutedEvent routed;

            protocol::Visit(
                event,
                [&]( const protocol::TextMessageEvent& value ) {
                    routed = RouteTextMessage( value, snapshot );
                },
                [&]( const protocol::ClientPresenceEvent& value ) {
                    const bool joined = value.kind == protocol::ClientPresenceKind::Joined;
                    routed.lines.push_back(
                        Line( ChannelConversation( value.channelId ),
                              FeedLineKind::Info,
                              value.clientName + ( joined ? " joined the channel" : " left the channel" ) ) );
                    routed.notification = audio::NotificationType::Message;
                },
                [&]( const protocol::CommandErrorEvent& value ) {
                    routed.lines.push_back(
                        Line( std::nullopt,
                              FeedLineKind::Error,
                              "server command failed (" + std::to_string( value.id ) + "): " + value.message ) );
                    routed.notification = audio::NotificationType::Failure;
                },
                [&]( const protocol::PokeEvent& value ) {
                    const std::string invoker =
                        value.invokerName.empty() ? "client " + std::to_string( value.invokerId ) : value.invokerName;
                    routed.lines.push_back(
                        Line( std::nullopt, FeedLineKind::Error, "poke from " + invoker + ": " + value.message ) );
                    routed.notification = audio::NotificationType::Reminder;
                },
                []( const protocol::VoiceEvent& ) {
                } );

            return routed;
        }

    } // namespace

    RoutedEvent RouteEvent( const RuntimeEvent& event, const ServerSnapshot& snapshot ) {
        RoutedEvent routed;

        std::visit(
            protocol::EventVisitor {
                [&]( const ProtocolEvent& value ) {
                    routed = RouteSessionEvent( value.event, snapshot );
                },
                [&]( const ActionErrorEvent& value ) {
                    routed.lines.push_back( Line( std::nullopt, FeedLineKind::Error, value.message ) );
                    routed.notification = audio::NotificationType::Failure;
                },
                [&]( const ActionInfoEvent& value ) {
                    routed.lines.push_back( Line( std::nullopt, FeedLineKind::Info, value.message ) );
                },
                [&]( const CurrentChannelChangedEvent& value ) {
                    routed.lines.push_back(
                        Line( ChannelConversation( value.channelId ), FeedLineKind::Info, "you joined " + value.name ) );
                    routed.notification = audio::NotificationType::Success;
                },
                [&]( const CurrentNicknameChangedEvent& value ) {
                    routed.lines.push_back( Line( std::nullopt, FeedLineKind::Info, "nickname is now " + value.nickname ) );
                },
                [&]( const DisconnectedEvent& value ) {
                    routed.lines.push_back( Line( std::nullopt, FeedLineKind::Error, DescribeDisconnect( value.info ) ) );
                    routed.notification = audio::NotificationType::Failure;
                },
                []( const ServerSnapshotEvent& ) {
                },
                []( const TalkStateEvent& ) {
                } },
            event );

        return routed;
    }

} // namespace ts::client::gtk
