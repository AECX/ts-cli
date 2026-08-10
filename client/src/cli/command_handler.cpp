#include <audio/audio_types.hpp>
#include <client/cli/command.hpp>
#include <client/cli/command_handler.hpp>
#include <client/cli/command_parser.hpp>
#include <client/cli/presentation.hpp>
#include <client/notification.hpp>
#include <client/runtime/action.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <optional>
#include <protocol/session/event.hpp>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace ts::client::cli {

    CommandHandler::CommandHandler( ActionQueue& actionQueue,
                                    Presentation& presentation,
                                    NotificationCallback notify,
                                    audio::AudioEngine* audio,
                                    PersistConfigCallback persistConfig ):
        m_ActionQueue( actionQueue ), m_Presentation( presentation ), m_Notify( std::move( notify ) ), m_Audio( audio ),
        m_PersistConfig( std::move( persistConfig ) ) {
    }

    void CommandHandler::Observe( const RuntimeEvent& event ) {
        if ( const auto* nicknameChanged = std::get_if<CurrentNicknameChangedEvent>( &event ); nicknameChanged != nullptr ) {
            if ( m_PersistConfig ) {
                try {
                    m_PersistConfig( [nickname = nicknameChanged->nickname]( Config& config ) {
                        config.SetNickname( nickname );
                    } );
                    m_Notify( ts::audio::NotificationType::Success );
                } catch ( const std::exception& exception ) {
                    m_Presentation.PrintError( exception.what() );
                }
            }
            return;
        }

        const auto* protocolEvent = std::get_if<ProtocolEvent>( &event );

        if ( protocolEvent == nullptr ) {
            return;
        }

        const auto* textMessage = std::get_if<protocol::TextMessageEvent>( &protocolEvent->event );

        if ( textMessage != nullptr && textMessage->replyTarget &&
             textMessage->replyTarget->mode == protocol::TextMessageTargetMode::Private ) {
            m_ReplyTarget = textMessage->replyTarget;
        }
    }

    void CommandHandler::PersistAudioSettings() {
        if ( m_PersistConfig ) {
            m_PersistConfig( [settings = m_Audio->Settings()]( Config& config ) {
                config.SetAudioSettings( settings );
            } );
        }
    }

    bool CommandHandler::HandleLine( std::string_view line ) {
        try {
            const std::optional<InputCommand> parsed = CommandParser::Parse( line );

            if ( !parsed ) {
                return true;
            }

            return std::visit(
                [this]( const auto& command ) -> bool {
                    using CommandType = std::decay_t<decltype( command )>;

                    if constexpr ( std::is_same_v<CommandType, SendChannelCommand> ) {
                        m_ActionQueue.Push( SendCurrentChannelMessageAction { .text = command.text } );
                    } else if constexpr ( std::is_same_v<CommandType, PrivateMessageCommand> ) {
                        m_ActionQueue.Push( SendPrivateMessageAction { .target = command.target, .text = command.text } );
                    } else if constexpr ( std::is_same_v<CommandType, ReplyCommand> ) {
                        if ( !m_ReplyTarget ) {
                            m_Presentation.PrintError( "There is no private message to reply to" );
                            return true;
                        }

                        m_ActionQueue.Push( SendReplyMessageAction { .target = *m_ReplyTarget, .text = command.text } );
                    } else if constexpr ( std::is_same_v<CommandType, JoinCommand> ) {
                        m_ActionQueue.Push( JoinChannelAction { .channel = command.channel } );
                    } else if constexpr ( std::is_same_v<CommandType, NickCommand> ) {
                        m_ActionQueue.Push( ChangeNicknameAction { .nickname = command.nickname } );
                    } else if constexpr ( std::is_same_v<CommandType, ListCommand> ) {
                        m_ActionQueue.Push( ListTreeAction { .start = command.start } );
                    } else if constexpr ( std::is_same_v<CommandType, UserCommand> ) {
                        m_ActionQueue.Push( UserSettingsAction { .target = command.target,
                                                                 .kind = command.kind,
                                                                 .numberValue = command.numberValue } );
                    } else if constexpr ( std::is_same_v<CommandType, AudioCommand> ) {
                        if ( m_Audio == nullptr ) {
                            m_Presentation.PrintError( "Audio subsystem is unavailable" );
                            return true;
                        }

                        switch ( command.kind ) {
                            case AudioCommandKind::Devices: {
                                const auto devices = m_Audio->Devices();
                                m_Presentation.PrintInfo( "audio device selector 'default' follows the system default" );

                                for ( const audio::DeviceKind kind : { audio::DeviceKind::Input, audio::DeviceKind::Output } ) {
                                    m_Presentation.PrintInfo( kind == audio::DeviceKind::Input ? "input devices:"
                                                                                               : "output devices:" );
                                    bool found = false;

                                    for ( const audio::AudioDevice& device : devices ) {
                                        if ( device.kind != kind ) {
                                            continue;
                                        }

                                        found = true;
                                        std::ostringstream line;
                                        line << "  [" << device.id << "] " << device.description;
                                        if ( device.name != device.description ) {
                                            line << " (" << device.name << ')';
                                        }
                                        m_Presentation.PrintInfo( line.str() );
                                    }

                                    if ( !found ) {
                                        m_Presentation.PrintInfo( "  (none discovered)" );
                                    }
                                }
                                break;
                            }

                            case AudioCommandKind::Status: {
                                const audio::AudioStatus status = m_Audio->Status();
                                std::ostringstream line;
                                line << "audio " << ( status.available ? "ready" : "unavailable" ) << ", input=" << status.input
                                     << ", output=" << status.output << ", filter=" << status.captureFilter
                                     << ", threshold=" << status.activationThresholdDb << " dBFS"
                                     << ", transmit=" << ( status.transmitEnabled ? "on" : "off" )
                                     << ", drops(capture/encoded/receive)=" << status.captureDrops << '/' << status.encodedDrops
                                     << '/' << status.receiveDrops;
                                if ( !status.error.empty() ) {
                                    line << ", error=" << status.error;
                                }
                                m_Presentation.PrintInfo( line.str() );
                                break;
                            }

                            case AudioCommandKind::Input:
                                m_Audio->SetInputDevice( command.value );
                                PersistAudioSettings();
                                m_Presentation.PrintInfo( "audio input set to " + command.value );
                                break;

                            case AudioCommandKind::Output:
                                m_Audio->SetOutputDevice( command.value );
                                PersistAudioSettings();
                                m_Presentation.PrintInfo( "audio output set to " + command.value );
                                break;

                            case AudioCommandKind::Filter:
                                if ( command.value.empty() ) {
                                    std::ostringstream line;
                                    line << "audio capture filters:";
                                    for ( const std::string& filter : m_Audio->CaptureFilters() ) {
                                        line << ' ' << filter;
                                    }
                                    m_Presentation.PrintInfo( line.str() );
                                } else {
                                    m_Audio->SetCaptureFilter( command.value );
                                    PersistAudioSettings();
                                    m_Presentation.PrintInfo( "audio capture filter set to " + command.value );
                                }
                                break;

                            case AudioCommandKind::Threshold: {
                                m_Audio->SetActivationThresholdDb( command.numberValue );
                                PersistAudioSettings();
                                std::ostringstream line;
                                line << "audio activation threshold set to " << command.numberValue << " dBFS";
                                m_Presentation.PrintInfo( line.str() );
                                break;
                            }

                            case AudioCommandKind::Transmit:
                                const bool enabled = command.value == "on";
                                m_Audio->SetTransmitEnabled( enabled );
                                m_Presentation.PrintInfo( enabled ? "microphone transmission enabled"
                                                                  : "microphone transmission disabled" );
                                if ( enabled ) {
                                    m_Notify( ts::audio::NotificationType::Success );
                                } else {
                                    m_Notify( ts::audio::NotificationType::Failure );
                                }
                                break;
                        }
                    } else if constexpr ( std::is_same_v<CommandType, ClearCommand> ) {
                        m_Presentation.Clear();
                    } else if constexpr ( std::is_same_v<CommandType, HelpCommand> ) {
                        m_Presentation.PrintInfo( "showing command help" );
                        m_Presentation.PrintHelp();
                    } else if constexpr ( std::is_same_v<CommandType, QuitCommand> ) {
                        m_Presentation.PrintInfo( "disconnecting" );
                        return false;
                    }

                    return true;
                },
                *parsed );
        } catch ( const std::runtime_error& exception ) {
            m_Presentation.PrintError( exception.what() );
            return true;
        }
    }

} // namespace ts::client::cli
