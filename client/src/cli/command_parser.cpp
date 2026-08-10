#include <cctype>
#include <charconv>
#include <client/cli/command.hpp>
#include <client/cli/command_parser.hpp>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ts::client::cli {

    std::optional<InputCommand> CommandParser::Parse( std::string_view line ) {
        line = Trim( line );

        if ( line.empty() ) {
            return std::nullopt;
        }

        if ( line.front() != '/' ) {
            return SendChannelCommand { .text = std::string( line ) };
        }

        line.remove_prefix( 1 );
        line = Trim( line );

        if ( line.empty() ) {
            throw std::runtime_error( "Empty command; use /help" );
        }

        const std::size_t separator = line.find_first_of( " \t" );

        const std::string_view commandName = separator == std::string_view::npos ? line : line.substr( 0, separator );

        const std::string_view arguments =
            separator == std::string_view::npos ? std::string_view {} : Trim( line.substr( separator + 1 ) );

        const std::string command = LowerAscii( commandName );

        if ( command == "pm" || command == "message" ) {
            return ParsePrivateMessage( arguments );
        }

        if ( command == "r" ) {
            if ( arguments.empty() ) {
                throw std::runtime_error( "Usage: /r <message>" );
            }

            return ReplyCommand { .text = std::string( arguments ) };
        }

        if ( command == "join" ) {
            return JoinCommand { .channel = ParseWholeValue( arguments, "Usage: /join <channel>" ) };
        }

        if ( command == "nick" ) {
            return NickCommand { .nickname = ParseWholeValue( arguments, "Usage: /nick <new name>" ) };
        }

        if ( command == "list" ) {
            if ( arguments.empty() ) {
                return ListCommand { .start = std::nullopt };
            }

            return ListCommand { .start = ParseWholeValue( arguments, "Usage: /list [channel]" ) };
        }

        if ( command == "user" ) {
            ParsedArgument target = ParseArgument( arguments );
            if ( target.value.empty() ) {
                throw std::runtime_error( "Usage: /user <client> [volume <dB|percent|reset>|mute|unmute]" );
            }

            if ( target.remainder.empty() ) {
                return UserCommand { .target = std::move( target.value ), .kind = UserCommandKind::Inspect };
            }

            ParsedArgument operation = ParseArgument( target.remainder );
            const std::string userCommand = LowerAscii( operation.value );

            if ( userCommand == "mute" || userCommand == "unmute" ) {
                if ( !operation.remainder.empty() ) {
                    throw std::runtime_error( "Usage: /user <client> mute|unmute" );
                }
                return UserCommand { .target = std::move( target.value ),
                                     .kind = userCommand == "mute" ? UserCommandKind::Mute : UserCommandKind::Unmute };
            }

            if ( userCommand == "volume" ) {
                const std::string value =
                    ParseWholeValue( operation.remainder, "Usage: /user <client> volume <dB|percent|reset>" );
                if ( LowerAscii( value ) == "reset" ) {
                    return UserCommand { .target = std::move( target.value ), .kind = UserCommandKind::VolumeReset };
                }

                if ( !value.empty() && value.back() == '%' ) {
                    const float percent = ParseFloatValue( std::string_view( value ).substr( 0, value.size() - 1 ),
                                                           "Usage: /user <client> volume <0..398%>" );
                    if ( percent < 0.0F || percent > 398.0F ) {
                        throw std::runtime_error( "User volume percent must be between 0 and 398" );
                    }
                    return UserCommand { .target = std::move( target.value ),
                                         .kind = UserCommandKind::VolumePercent,
                                         .numberValue = percent };
                }

                const float volumeDb = ParseFloatValue( value, "Usage: /user <client> volume <-60..12 dB>" );
                if ( volumeDb < -60.0F || volumeDb > 12.0F ) {
                    throw std::runtime_error( "User volume must be between -60 and +12 dB" );
                }
                return UserCommand { .target = std::move( target.value ),
                                     .kind = UserCommandKind::VolumeDb,
                                     .numberValue = volumeDb };
            }

            throw std::runtime_error( "Usage: /user <client> [volume <dB|percent|reset>|mute|unmute]" );
        }

        if ( command == "mute" || command == "unmute" ) {
            if ( !arguments.empty() ) {
                throw std::runtime_error( command == "mute" ? "Usage: /mute" : "Usage: /unmute" );
            }

            return AudioCommand { .kind = AudioCommandKind::Transmit, .value = command == "unmute" ? "on" : "off" };
        }

        if ( command == "audio" ) {
            if ( arguments.empty() ) {
                return AudioCommand { .kind = AudioCommandKind::Status, .value = {} };
            }

            ParsedArgument subcommand = ParseArgument( arguments );
            const std::string audioCommand = LowerAscii( subcommand.value );

            if ( audioCommand == "devices" ) {
                if ( !subcommand.remainder.empty() ) {
                    throw std::runtime_error( "Usage: /audio devices" );
                }
                return AudioCommand { .kind = AudioCommandKind::Devices, .value = {} };
            }

            if ( audioCommand == "status" ) {
                if ( !subcommand.remainder.empty() ) {
                    throw std::runtime_error( "Usage: /audio status" );
                }
                return AudioCommand { .kind = AudioCommandKind::Status, .value = {} };
            }

            if ( audioCommand == "input" ) {
                return AudioCommand { .kind = AudioCommandKind::Input,
                                      .value =
                                          ParseWholeValue( subcommand.remainder, "Usage: /audio input <default|id|name>" ) };
            }

            if ( audioCommand == "output" ) {
                return AudioCommand { .kind = AudioCommandKind::Output,
                                      .value =
                                          ParseWholeValue( subcommand.remainder, "Usage: /audio output <default|id|name>" ) };
            }

            if ( audioCommand == "filter" ) {
                if ( subcommand.remainder.empty() ) {
                    return AudioCommand { .kind = AudioCommandKind::Filter, .value = {} };
                }

                return AudioCommand {
                    .kind = AudioCommandKind::Filter,
                    .value = LowerAscii( ParseWholeValue( subcommand.remainder, "Usage: /audio filter [none|name]" ) ) };
            }

            if ( audioCommand == "threshold" ) {
                const float threshold = ParseFloatValue( subcommand.remainder, "Usage: /audio threshold <-100..0 dBFS>" );
                if ( threshold < -100.0F || threshold > 0.0F ) {
                    throw std::runtime_error( "Usage: /audio threshold <-100..0 dBFS>" );
                }
                return AudioCommand { .kind = AudioCommandKind::Threshold, .value = {}, .numberValue = threshold };
            }

            if ( audioCommand == "transmit" ) {
                const std::string value =
                    LowerAscii( ParseWholeValue( subcommand.remainder, "Usage: /audio transmit <on|off>" ) );
                if ( value != "on" && value != "off" ) {
                    throw std::runtime_error( "Usage: /audio transmit <on|off>" );
                }
                return AudioCommand { .kind = AudioCommandKind::Transmit, .value = value };
            }

            throw std::runtime_error( "Unknown /audio command; use /help" );
        }

        if ( command == "clear" ) {
            return ClearCommand {};
        }

        if ( command == "help" ) {
            if ( !arguments.empty() ) {
                throw std::runtime_error( "Usage: /help" );
            }

            return HelpCommand {};
        }

        if ( command == "quit" || command == "exit" ) {
            if ( !arguments.empty() ) {
                throw std::runtime_error( "Usage: /quit" );
            }

            return QuitCommand {};
        }

        throw std::runtime_error( "Unknown command: /" + command + "; use /help" );
    }

    std::string_view CommandParser::Trim( std::string_view value ) {
        while ( !value.empty() && std::isspace( static_cast<unsigned char>( value.front() ) ) != 0 ) {
            value.remove_prefix( 1 );
        }

        while ( !value.empty() && std::isspace( static_cast<unsigned char>( value.back() ) ) != 0 ) {
            value.remove_suffix( 1 );
        }

        return value;
    }

    std::string CommandParser::LowerAscii( std::string_view value ) {
        std::string result;
        result.reserve( value.size() );

        for ( const char character : value ) {
            const auto byte = static_cast<unsigned char>( character );
            result.push_back( static_cast<char>( std::tolower( byte ) ) );
        }

        return result;
    }

    CommandParser::ParsedArgument CommandParser::ParseArgument( std::string_view value ) {
        value = Trim( value );

        if ( value.empty() ) {
            throw std::runtime_error( "Missing command argument" );
        }

        if ( value.front() != '"' ) {
            const std::size_t separator = value.find_first_of( " \t" );

            if ( separator == std::string_view::npos ) {
                return ParsedArgument { .value = std::string( value ), .remainder = {} };
            }

            return ParsedArgument { .value = std::string( value.substr( 0, separator ) ),
                                    .remainder = Trim( value.substr( separator + 1 ) ) };
        }

        std::string result;
        bool escaped = false;

        for ( std::size_t index = 1; index < value.size(); ++index ) {
            const char character = value[index];

            if ( escaped ) {
                result.push_back( character );
                escaped = false;
                continue;
            }

            if ( character == '\\' ) {
                escaped = true;
                continue;
            }

            if ( character == '"' ) {
                return ParsedArgument { .value = std::move( result ), .remainder = Trim( value.substr( index + 1 ) ) };
            }

            result.push_back( character );
        }

        throw std::runtime_error( "Unterminated quoted command argument" );
    }

    std::string CommandParser::ParseWholeValue( std::string_view value, std::string_view usage ) {
        value = Trim( value );

        if ( value.empty() ) {
            throw std::runtime_error( std::string( usage ) );
        }

        if ( value.front() != '"' ) {
            return std::string( value );
        }

        ParsedArgument parsed = ParseArgument( value );

        if ( !parsed.remainder.empty() ) {
            throw std::runtime_error( std::string( usage ) );
        }

        if ( parsed.value.empty() ) {
            throw std::runtime_error( std::string( usage ) );
        }

        return parsed.value;
    }

    float CommandParser::ParseFloatValue( std::string_view value, std::string_view usage ) {
        value = Trim( value );

        if ( value.empty() ) {
            throw std::runtime_error( std::string( usage ) );
        }

        float result = 0.0F;
        const char* begin = value.data();
        const char* end = value.data() + value.size();
        const auto parsed = std::from_chars( begin, end, result );

        if ( parsed.ec != std::errc {} || parsed.ptr != end || !std::isfinite( result ) ) {
            throw std::runtime_error( std::string( usage ) );
        }

        return result;
    }

    InputCommand CommandParser::ParsePrivateMessage( std::string_view arguments ) {
        if ( arguments.empty() ) {
            throw std::runtime_error( "Usage: /pm|/message <client> <message>" );
        }

        ParsedArgument target = ParseArgument( arguments );

        if ( target.value.empty() || target.remainder.empty() ) {
            throw std::runtime_error( "Usage: /pm|/message <client> <message>" );
        }

        return PrivateMessageCommand { .target = std::move( target.value ), .text = std::string( target.remainder ) };
    }

} // namespace ts::client::cli
