#include "test_support.hpp"

#include <audio/audio_types.hpp>
#include <client/cli/command_handler.hpp>
#include <client/cli/presentation.hpp>
#include <client/notification.hpp>
#include <client/runtime/action.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <sstream>
#include <string>
#include <variant>

namespace ts::client::test {

    void RunCommandHandlerTests() {
        ActionQueue actionQueue;
        std::ostringstream output;
        cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
        } );
        cli::CommandHandler handler( actionQueue, presentation, [&]( ts::audio::NotificationType ) {
        } );

        Expect( handler.HandleLine( "/r hello" ), "/r without a target unexpectedly requested exit" );

        ClientAction action;
        Expect( !actionQueue.TryPop( action ), "/r without a received message queued an action" );

        const RuntimeEvent channelEvent = ProtocolEvent {
            .event = protocol::TextMessageEvent {
                .message = protocol::TextMessageEntry { .targetMode = protocol::TextMessageTargetMode::Channel,
                                                        .targetId = 7,
                                                        .invokerId = 12,
                                                        .invokerName = "ChannelUser",
                                                        .invokerUniqueId = "uid-channel",
                                                        .text = "hello channel" },
                .replyTarget = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Channel, .id = 7 },
                .channelName = "Lobby",
                .privatePeerName = {},
                .outgoing = false } };

        handler.Observe( channelEvent );
        Expect( handler.HandleLine( "/r still private" ), "/r after a channel message unexpectedly requested exit" );
        Expect( !actionQueue.TryPop( action ), "/r accepted a channel message as a reply target" );

        const RuntimeEvent event = ProtocolEvent {
            .event = protocol::TextMessageEvent {
                .message = protocol::TextMessageEntry { .targetMode = protocol::TextMessageTargetMode::Private,
                                                        .targetId = std::nullopt,
                                                        .invokerId = 11,
                                                        .invokerName = "User",
                                                        .invokerUniqueId = "uid",
                                                        .text = "hey" },
                .replyTarget = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = 11 },
                .channelName = {},
                .privatePeerName = "User",
                .outgoing = false } };

        handler.Observe( event );

        Expect( handler.HandleLine( "/r hello back" ), "/r unexpectedly requested exit" );

        Expect( actionQueue.TryPop( action ), "/r did not queue a reply action" );

        const auto* reply = std::get_if<SendReplyMessageAction>( &action );

        Expect( reply != nullptr, "/r queued the wrong action type" );

        Expect( reply->target.mode == protocol::TextMessageTargetMode::Private, "/r changed the remembered target mode" );

        ExpectEqual( reply->target.id, std::uint64_t { 11 }, "/r changed the remembered target ID" );

        ExpectEqual( reply->text, std::string( "hello back" ), "/r changed the reply text" );

        Expect( !handler.HandleLine( "/quit" ), "/quit did not request exit" );

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            ActionQueue actionQueue;
            audio::AudioEngine audio;
            bool persisted = false;
            Config scratch = Config::Create( Config::DefaultProfile() );

            cli::CommandHandler handler(
                actionQueue,
                presentation,
                [&]( ts::audio::NotificationType ) {
                },
                &audio,
                [&persisted, &scratch]( const ConfigMutator& mutator ) {
                    persisted = true;
                    mutator( scratch );
                } );

            Expect( handler.HandleLine( "/audio threshold -40" ), "/audio threshold unexpectedly requested exit" );
            Expect( persisted, "/audio threshold did not persist its setting" );
            Expect( scratch.AudioSettings().activationThresholdDb == -40.0F, "/audio threshold persisted the wrong value" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            ActionQueue actionQueue;
            Config scratch = Config::Create( Config::DefaultProfile() );

            cli::CommandHandler handler(
                actionQueue,
                presentation,
                [&]( ts::audio::NotificationType ) {
                },
                nullptr,
                [&scratch]( const ConfigMutator& mutator ) {
                    mutator( scratch );
                } );

            handler.Observe( CurrentNicknameChangedEvent { .nickname = "newnick" } );
            ExpectEqual( scratch.Profile().nickname, std::string( "newnick" ), "Nickname change was not persisted" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            ActionQueue actionQueue;
            bool called = false;

            cli::CommandHandler handler(
                actionQueue,
                presentation,
                [&]( ts::audio::NotificationType ) {
                },
                nullptr,
                [&called]( const ConfigMutator& mutator ) {
                    called = true;
                    Config scratch = Config::Create( Config::DefaultProfile() );
                    mutator( scratch );
                } );

            handler.Observe( CurrentNicknameChangedEvent { .nickname = "" } );
            Expect( called, "Nickname persistence callback was not invoked" );
            Expect( output.str().find( "cannot be empty" ) != std::string::npos,
                    "Observe() did not report a persistence failure instead of throwing" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            ActionQueue actionQueue;
            cli::CommandHandler handler( actionQueue, presentation, [&]( ts::audio::NotificationType ) {
            } );

            Expect( handler.HandleLine( "/user \"Loud User\" volume -9" ), "/user volume unexpectedly requested exit" );
            ClientAction action;
            Expect( actionQueue.TryPop( action ), "/user volume did not queue an action" );
            const auto* user = std::get_if<UserSettingsAction>( &action );
            Expect( user != nullptr, "/user volume queued the wrong action" );
            ExpectEqual( user->target, std::string( "Loud User" ), "/user volume changed the target" );
            Expect( user->kind == cli::UserCommandKind::VolumeDb && user->numberValue == -9.0F,
                    "/user volume changed the requested setting" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            ActionQueue actionQueue;
            cli::CommandHandler handler( actionQueue, presentation, [&]( ts::audio::NotificationType ) {
            } );

            Expect( handler.HandleLine( "/list Lobby" ), "/list unexpectedly requested exit" );

            ClientAction action;
            Expect( actionQueue.TryPop( action ), "/list did not queue an action" );
            const auto* list = std::get_if<ListTreeAction>( &action );
            Expect( list != nullptr, "/list queued the wrong action" );
            Expect( list->start.has_value(), "/list lost its starting channel" );
            ExpectEqual( *list->start, std::string( "Lobby" ), "/list changed its starting channel" );
        }
    }

} // namespace ts::client::test
