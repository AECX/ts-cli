#include <client/cli/presentation.hpp>
#include <client/platform/terminal_control.hpp>
#include <client/runtime/event.hpp>
#include <cstdint>
#include <functional>
#include <ostream>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace ts::client::cli {

    Presentation::Presentation( std::ostream& output, bool useColor, NotificationCallback notify ):
        m_Output( output ), m_UseColor( useColor ), m_Notify( std::move( notify ) ) {
    }

    void Presentation::Print( const RuntimeEvent& event ) {
        std::visit(
            [this]( const auto& value ) {
                using EventType = std::decay_t<decltype( value )>;

                if constexpr ( std::is_same_v<EventType, ProtocolEvent> ) {
                    Print( value.event );
                } else if constexpr ( std::is_same_v<EventType, ActionErrorEvent> ) {
                    PrintError( value.message );
                } else if constexpr ( std::is_same_v<EventType, ActionInfoEvent> ) {
                    PrintInfo( value.message );
                } else if constexpr ( std::is_same_v<EventType, CurrentChannelChangedEvent> ) {
                    SetCurrentChannel( value.name );
                    PrintInfo( "now in " + value.name );
                    m_Notify( ts::audio::NotificationType::Success );
                } else if constexpr ( std::is_same_v<EventType, CurrentNicknameChangedEvent> ) {
                    SetCurrentNickname( value.nickname );
                    PrintInfo( "nickname is now " + value.nickname );
                }
            },
            event );
    }

    void Presentation::SetCurrentChannel( std::string_view channel ) {
        m_CurrentChannel = std::string( channel );
    }

    void Presentation::SetCurrentNickname( std::string_view nickname ) {
        m_CurrentNickname = std::string( nickname );
    }

    void Presentation::PrintPrompt() {
        if ( m_UseColor ) {
            log::ApplyTerminalStyle( m_Output, log::TerminalStream::StandardOutput, { .color = log::TerminalColor::Green } );
        }

        m_Output << '[';
        WriteSafe( m_CurrentChannel.empty() ? std::string_view { "?" } : std::string_view { m_CurrentChannel } );
        m_Output << "] ";
        WriteSafe( m_CurrentNickname.empty() ? std::string_view { "?" } : std::string_view { m_CurrentNickname } );

        if ( m_UseColor ) {
            log::ResetTerminalStyle( m_Output, log::TerminalStream::StandardOutput );
        }

        m_Output << " > " << std::flush;
    }

    void Presentation::BreakPromptLine() {
        if ( m_UseColor ) {
            platform::ClearCurrentLine( m_Output );
            return;
        }

        m_Output << std::endl;
    }

    void Presentation::ClearSubmittedInputLine() {
        if ( !m_UseColor ) {
            return;
        }

        /*
         * stdin is in the terminal's normal canonical/echo mode. After Enter,
         * the terminal has already echoed the prompt plus submitted text and
         * moved the cursor to the next row. Remove that input row so chat is
         * rendered only from the server's notifytextmessage event.
         */
        platform::ClearPreviousLine( m_Output );
    }

    void Presentation::Print( const protocol::SessionEvent& event ) {
        std::visit(
            [this]( const auto& value ) {
                using EventType = std::decay_t<decltype( value )>;

                if constexpr ( std::is_same_v<EventType, protocol::TextMessageEvent> ) {
                    PrintTextMessage( value );
                } else if constexpr ( std::is_same_v<EventType, protocol::ClientPresenceEvent> ) {
                    PrintPresence( value );
                } else if constexpr ( std::is_same_v<EventType, protocol::CommandErrorEvent> ) {
                    PrintCommandError( value );
                }
            },
            event );
    }

    void Presentation::Clear() {
        platform::ClearScreen( m_Output );
    }

    void Presentation::PrintHelp() {
        m_Output << "Commands:" << std::endl
                 << "  <text>                         send to the current channel" << std::endl
                 << "  /pm <client> <text>            private message; clients only" << std::endl
                 << "  /message <client> <text>       alias for /pm" << std::endl
                 << "  /r <text>                      reply to the last received private message" << std::endl
                 << "  /join <channel>                move to a channel" << std::endl
                 << "  /nick <new name>               change nickname" << std::endl
                 << "  /list [channel]                show channel/client tree and status" << std::endl
                 << "  /user <client>                  show persistent local settings for a client" << std::endl
                 << "  /user <client> volume <dB|%>    set that client's playback volume" << std::endl
                 << "  /user <client> volume reset     restore normal playback volume" << std::endl
                 << "  /user <client> mute|unmute      locally mute or unmute that client" << std::endl
                 << "  /audio devices                  list PipeWire input/output devices" << std::endl
                 << "  /audio status                   show audio status" << std::endl
                 << "  /audio input <default|id|name>  select microphone" << std::endl
                 << "  /audio output <default|id|name> select playback device" << std::endl
                 << "  /audio filter                   list capture filters" << std::endl
                 << "  /audio filter <none|name>       select capture filter" << std::endl
                 << "  /audio threshold <-100..0>      set microphone activation threshold in dBFS" << std::endl
                 << "  /mute                           mute microphone transmission" << std::endl
                 << "  /unmute                         unmute microphone transmission" << std::endl
                 << "  /audio transmit <on|off>        aliases for /unmute and /mute" << std::endl
                 << "  /clear                          clears the current output buffer" << std::endl
                 << "  /help                           show this help" << std::endl
                 << "  /quit                           disconnect and exit" << std::endl
                 << "Quote client names containing spaces. Use #<client-id> to disambiguate clients." << std::endl;
    }

    void Presentation::PrintError( std::string_view message ) {
        m_Notify( ts::audio::NotificationType::Failure );
        if ( m_UseColor ) {
            log::ApplyTerminalStyle( m_Output, log::TerminalStream::StandardOutput, { .color = log::TerminalColor::Red } );
        }

        m_Output << "[error] ";
        WriteSafe( message );

        if ( m_UseColor ) {
            log::ResetTerminalStyle( m_Output, log::TerminalStream::StandardOutput );
        }

        m_Output << std::endl;
    }

    void Presentation::PrintInfo( std::string_view message ) {
        if ( m_UseColor ) {
            log::ApplyTerminalStyle( m_Output,
                                     log::TerminalStream::StandardOutput,
                                     { .color = log::TerminalColor::Cyan, .italic = true } );
        }

        m_Output << "* ";
        WriteSafe( message );

        if ( m_UseColor ) {
            log::ResetTerminalStyle( m_Output, log::TerminalStream::StandardOutput );
        }

        m_Output << std::endl;
    }

    void Presentation::PrintTextMessage( const protocol::TextMessageEvent& event ) {
        if ( !event.outgoing ) {
            m_Notify( ts::audio::NotificationType::Message );
        }

        const protocol::TextMessageEntry& message = event.message;

        std::string sender = message.invokerName;

        if ( sender.empty() ) {
            sender = event.outgoing && !m_CurrentNickname.empty() ? m_CurrentNickname
                                                                  : "client " + std::to_string( message.invokerId );
        }

        if ( m_UseColor ) {
            log::ApplyTerminalStyle( m_Output, log::TerminalStream::StandardOutput, { .color = Color( message.targetMode ) } );
        }

        switch ( message.targetMode ) {
            case protocol::TextMessageTargetMode::Private: {

                WriteSafe( event.outgoing ? "[to " : "[from " );
                WriteSafe( event.privatePeerName.empty() ? sender : event.privatePeerName );
                WriteSafe( "]" );
                break;
            }

            case protocol::TextMessageTargetMode::Channel: {
                std::string_view channelName = event.channelName;

                if ( channelName.empty() ) {
                    channelName = m_CurrentChannel.empty() ? std::string_view { "?" } : std::string_view { m_CurrentChannel };
                }

                m_Output << '[';
                WriteSafe( channelName );
                m_Output << "] ";
                WriteSafe( sender );
                break;
            }

            case protocol::TextMessageTargetMode::Server:
                m_Output << "[SERVER] ";
                WriteSafe( sender );
                break;
        }

        if ( m_UseColor ) {
            log::ResetTerminalStyle( m_Output, log::TerminalStream::StandardOutput );
        }

        m_Output << ( event.outgoing ? " > " : " < " );
        WriteSafe( message.text );
        m_Output << std::endl;
    }

    void Presentation::PrintPresence( const protocol::ClientPresenceEvent& event ) {
        if ( m_UseColor ) {
            // Cyan + italic keeps presence distinct from normal green chat.
            log::ApplyTerminalStyle( m_Output,
                                     log::TerminalStream::StandardOutput,
                                     { .color = log::TerminalColor::Cyan, .italic = true } );
        }

        m_Output << '[';
        WriteSafe( event.channelName.empty() ? std::string_view { "?" } : std::string_view { event.channelName } );
        m_Output << "] ";
        WriteSafe( event.clientName );
        m_Output << ": " << ( event.kind == protocol::ClientPresenceKind::Joined ? "joined the channel" : "left the channel" );

        m_Notify( ts::audio::NotificationType::Message );

        if ( m_UseColor ) {
            log::ResetTerminalStyle( m_Output, log::TerminalStream::StandardOutput );
        }

        m_Output << std::endl;
    }

    void Presentation::PrintCommandError( const protocol::CommandErrorEvent& event ) {
        std::string message = "server command failed (";
        message += std::to_string( event.id );
        message += "): ";
        message += event.message;

        PrintError( message );
    }

    void Presentation::WriteSafe( std::string_view value ) {
        for ( const char character : value ) {
            const auto byte = static_cast<unsigned char>( character );

            if ( byte < 0x20 || byte == 0x7f ) {
                m_Output << ' ';
                continue;
            }

            m_Output << character;
        }
    }

    log::TerminalColor Presentation::Color( protocol::TextMessageTargetMode targetMode ) {
        switch ( targetMode ) {
            case protocol::TextMessageTargetMode::Private:
                return log::TerminalColor::Magenta;

            case protocol::TextMessageTargetMode::Channel:
                return log::TerminalColor::Green;

            case protocol::TextMessageTargetMode::Server:
                return log::TerminalColor::Yellow;
        }

        return log::TerminalColor::Green;
    }

} // namespace ts::client::cli
