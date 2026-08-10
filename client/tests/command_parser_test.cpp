#include "test_support.hpp"

#include <client/cli/command.hpp>
#include <client/cli/command_parser.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace ts::client::test {

    void RunCommandParserTests() {
        {
            const auto parsed = cli::CommandParser::Parse( "hello channel" );
            Expect( parsed.has_value(), "Plain text did not produce a command" );

            const auto* command = std::get_if<cli::SendChannelCommand>( &*parsed );
            Expect( command != nullptr, "Plain text did not become a channel message" );
            ExpectEqual( command->text, std::string( "hello channel" ), "Plain channel message changed" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/pm \"User Smith\" hey buddy" );
            const auto* command = std::get_if<cli::PrivateMessageCommand>( &*parsed );

            Expect( command != nullptr, "/pm parsed to the wrong command type" );
            ExpectEqual( command->target, std::string( "User Smith" ), "/pm parsed the wrong target" );
            ExpectEqual( command->text, std::string( "hey buddy" ), "/pm parsed the wrong message" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/message \"User Smith\" hello there" );
            const auto* command = std::get_if<cli::PrivateMessageCommand>( &*parsed );

            Expect( command != nullptr, "/message did not behave like /pm" );
            ExpectEqual( command->target, std::string( "User Smith" ), "/message parsed the wrong target" );
            ExpectEqual( command->text, std::string( "hello there" ), "/message parsed the wrong message" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/list \"Games Room\"" );
            const auto* command = std::get_if<cli::ListCommand>( &*parsed );

            Expect( command != nullptr, "/list parsed to the wrong command type" );
            Expect( command->start.has_value(), "/list lost the starting node" );
            ExpectEqual( *command->start, std::string( "Games Room" ), "/list changed the starting node" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/list" );
            const auto* command = std::get_if<cli::ListCommand>( &*parsed );

            Expect( command != nullptr, "/list without an argument parsed to the wrong command type" );
            Expect( !command->start.has_value(), "/list without an argument unexpectedly has a starting node" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/r yep" );
            const auto* command = std::get_if<cli::ReplyCommand>( &*parsed );

            Expect( command != nullptr, "/r parsed to the wrong command type" );
            ExpectEqual( command->text, std::string( "yep" ), "/r parsed the wrong message" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/join Konferenzraum 1" );
            const auto* command = std::get_if<cli::JoinCommand>( &*parsed );

            Expect( command != nullptr, "/join parsed to the wrong command type" );
            ExpectEqual( command->channel,
                         std::string( "Konferenzraum 1" ),
                         "/join did not preserve a channel name containing spaces" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/nick new name" );
            const auto* command = std::get_if<cli::NickCommand>( &*parsed );

            Expect( command != nullptr, "/nick parsed to the wrong command type" );
            ExpectEqual( command->nickname, std::string( "new name" ), "/nick did not preserve the nickname" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio devices" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio devices parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Devices, "/audio devices parsed to the wrong operation" );
            Expect( command->value.empty(), "/audio devices unexpectedly carried a value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio input \"Built-in Audio Analog Stereo\"" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio input parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Input, "/audio input parsed to the wrong operation" );
            ExpectEqual( command->value,
                         std::string( "Built-in Audio Analog Stereo" ),
                         "/audio input did not preserve a quoted device name" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio output default" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio output parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Output, "/audio output parsed to the wrong operation" );
            ExpectEqual( command->value, std::string( "default" ), "/audio output changed the default selector" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio filter" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio filter parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Filter, "/audio filter parsed to the wrong operation" );
            Expect( command->value.empty(), "/audio filter unexpectedly carried a value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio filter RNNoise" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio filter RNNoise parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Filter, "/audio filter RNNoise parsed to the wrong operation" );
            ExpectEqual( command->value, std::string( "rnnoise" ), "/audio filter did not normalize its value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio threshold -42.5" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio threshold parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Threshold, "/audio threshold parsed to the wrong operation" );
            Expect( command->numberValue == -42.5F, "/audio threshold changed its value" );
        }

        {
            bool rejected = false;

            try {
                (void)cli::CommandParser::Parse( "/audio threshold 3" );
            } catch ( const std::runtime_error& ) {
                rejected = true;
            }

            Expect( rejected, "/audio threshold accepted an invalid positive dBFS value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/audio transmit on" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/audio transmit parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Transmit, "/audio transmit parsed to the wrong operation" );
            ExpectEqual( command->value, std::string( "on" ), "/audio transmit changed its value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/mute" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/mute parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Transmit, "/mute parsed to the wrong operation" );
            ExpectEqual( command->value, std::string( "off" ), "/mute did not disable transmission" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/unmute" );
            const auto* command = std::get_if<cli::AudioCommand>( &*parsed );

            Expect( command != nullptr, "/unmute parsed to the wrong command type" );
            Expect( command->kind == cli::AudioCommandKind::Transmit, "/unmute parsed to the wrong operation" );
            ExpectEqual( command->value, std::string( "on" ), "/unmute did not enable transmission" );
        }

        {
            bool rejected = false;

            try {
                (void)cli::CommandParser::Parse( "/audio transmit maybe" );
            } catch ( const std::runtime_error& ) {
                rejected = true;
            }

            Expect( rejected, "/audio transmit accepted an invalid state" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/user \"Loud User\" volume -8.5" );
            const auto* command = std::get_if<cli::UserCommand>( &*parsed );

            Expect( command != nullptr, "/user volume parsed to the wrong command type" );
            ExpectEqual( command->target, std::string( "Loud User" ), "/user volume changed its target" );
            Expect( command->kind == cli::UserCommandKind::VolumeDb, "/user volume parsed to the wrong operation" );
            Expect( command->numberValue == -8.5F, "/user volume changed its dB value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/user #42 volume 75%" );
            const auto* command = std::get_if<cli::UserCommand>( &*parsed );

            Expect( command != nullptr, "/user percent parsed to the wrong command type" );
            ExpectEqual( command->target, std::string( "#42" ), "/user percent changed its target" );
            Expect( command->kind == cli::UserCommandKind::VolumePercent, "/user percent parsed to the wrong operation" );
            Expect( command->numberValue == 75.0F, "/user percent changed its value" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/user Alice mute" );
            const auto* command = std::get_if<cli::UserCommand>( &*parsed );
            Expect( command != nullptr && command->kind == cli::UserCommandKind::Mute, "/user mute parsed incorrectly" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "/user Alice volume reset" );
            const auto* command = std::get_if<cli::UserCommand>( &*parsed );
            Expect( command != nullptr && command->kind == cli::UserCommandKind::VolumeReset,
                    "/user volume reset parsed incorrectly" );
        }

        {
            const auto parsed = cli::CommandParser::Parse( "   " );
            Expect( !parsed.has_value(), "Blank input unexpectedly produced a command" );
        }
    }

} // namespace ts::client::test
