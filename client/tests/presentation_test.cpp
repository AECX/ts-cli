#include "test_support.hpp"

#include <client/cli/presentation.hpp>
#include <client/notification.hpp>
#include <client/runtime/event.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <sstream>
#include <string>
#include <utility>

namespace ts::client::test {

    protocol::SessionEvent TextEvent( protocol::TextMessageTargetMode targetMode,
                                      std::uint16_t invokerId,
                                      std::string invokerName,
                                      std::string text,
                                      bool outgoing = false,
                                      std::string privatePeerName = {} ) {
        return protocol::TextMessageEvent {
            .message = protocol::TextMessageEntry { .targetMode = targetMode,
                                                    .targetId = std::nullopt,
                                                    .invokerId = invokerId,
                                                    .invokerName = std::move( invokerName ),
                                                    .invokerUniqueId = {},
                                                    .text = std::move( text ) },
            .replyTarget = std::nullopt,
            .channelName = targetMode == protocol::TextMessageTargetMode::Channel ? std::string { "Lobby" } : std::string {},
            .privatePeerName = std::move( privatePeerName ),
            .outgoing = outgoing };
    }

    void RunPresentationTests() {
        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.Print( TextEvent( protocol::TextMessageTargetMode::Channel, 11, "User", "hey buddy whats up?" ) );

            ExpectEqual( output.str(),
                         std::string( "[Lobby] User < hey buddy whats up?\n" ),
                         "Channel message presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.Print( RuntimeEvent { ActionErrorEvent { .message = "No client matches 'Nobody'" } } );

            ExpectEqual( output.str(),
                         std::string( "[error] No client matches 'Nobody'\n" ),
                         "Action error presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.Print( TextEvent( protocol::TextMessageTargetMode::Private, 11, "User", "secret" ) );

            ExpectEqual( output.str(), std::string( "[from User] < secret\n" ), "Private message presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            presentation.SetCurrentNickname( "frAgZ" );

            presentation.Print( TextEvent( protocol::TextMessageTargetMode::Channel, 7, "frAgZ", "testmessage", true ) );

            ExpectEqual( output.str(),
                         std::string( "[Lobby] frAgZ > testmessage\n" ),
                         "Outgoing channel message presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );
            presentation.SetCurrentNickname( "frAgZ" );

            presentation.Print( TextEvent( protocol::TextMessageTargetMode::Private, 7, "frAgZ", "secret", true, "User" ) );

            ExpectEqual( output.str(),
                         std::string( "[to User] > secret\n" ),
                         "Outgoing private message presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.Print(
                protocol::SessionEvent { protocol::ClientPresenceEvent { .kind = protocol::ClientPresenceKind::Joined,
                                                                         .clientId = 11,
                                                                         .channelId = 1,
                                                                         .clientName = "User",
                                                                         .channelName = "Lobby" } } );
            presentation.Print(
                protocol::SessionEvent { protocol::ClientPresenceEvent { .kind = protocol::ClientPresenceKind::Left,
                                                                         .clientId = 11,
                                                                         .channelId = 1,
                                                                         .clientName = "User",
                                                                         .channelName = "Lobby" } } );

            ExpectEqual( output.str(),
                         std::string( "[Lobby] User: joined the channel\n[Lobby] User: left the channel\n" ),
                         "Presence presentation is incorrect" );
        }

#ifndef _WIN32
        /*
         * Windows renders color/cursor control through native console API
         * calls on the real console handle (see log::ApplyTerminalStyle,
         * client::platform::ClearPreviousLine) rather than by embedding
         * ANSI bytes in the stream, so these exact-byte assertions only
         * apply to the POSIX backend; a plain std::ostringstream isn't a
         * real console for the Windows path to act on.
         */
        {
            std::ostringstream output;
            cli::Presentation presentation( output, true );

            presentation.Print(
                protocol::SessionEvent { protocol::ClientPresenceEvent { .kind = protocol::ClientPresenceKind::Joined,
                                                                         .clientId = 11,
                                                                         .channelId = 1,
                                                                         .clientName = "User",
                                                                         .channelName = "Lobby" } } );

            ExpectEqual( output.str(),
                         std::string( "\x1b[36;3m[Lobby] User: joined the channel\x1b[0m\n" ),
                         "Colored presence presentation is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, true );

            presentation.ClearSubmittedInputLine();

            ExpectEqual( output.str(), std::string( "\x1b[1A\r\x1b[2K" ), "Submitted input cleanup is incorrect" );
        }
#endif

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.SetCurrentChannel( "Lobby" );
            presentation.SetCurrentNickname( "aecx" );
            presentation.PrintPrompt();

            ExpectEqual( output.str(), std::string( "[Lobby] aecx > " ), "Current identity prompt is incorrect" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.SetCurrentChannel( "Lobby" );
            presentation.SetCurrentNickname( "aecx" );
            presentation.Print( RuntimeEvent { CurrentChannelChangedEvent { .channelId = 7, .name = "Games" } } );
            presentation.PrintPrompt();

            ExpectEqual( output.str(),
                         std::string( "* now in Games\n[Games] aecx > " ),
                         "Channel-change prompt did not preserve the current nickname" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.SetCurrentChannel( "Lobby" );
            presentation.SetCurrentNickname( "aecx" );
            presentation.Print( RuntimeEvent { CurrentNicknameChangedEvent { .nickname = "User" } } );
            presentation.PrintPrompt();

            ExpectEqual( output.str(),
                         std::string( "* nickname is now User\n[Lobby] User > " ),
                         "Nickname-change prompt did not track the confirmed current nickname" );
        }

        {
            std::ostringstream output;
            cli::Presentation presentation( output, false, [&]( ts::audio::NotificationType ) {
            } );

            presentation.Print( TextEvent( protocol::TextMessageTargetMode::Server, 11, "User", "maintenance" ) );

            ExpectEqual( output.str(),
                         std::string( "[SERVER] User < maintenance\n" ),
                         "Server message presentation is incorrect" );
        }
    }

} // namespace ts::client::test
