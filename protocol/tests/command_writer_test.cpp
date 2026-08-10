#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <protocol/command/parser.hpp>
#include <protocol/command/writer.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ts::test {

    void RunCommandWriterTests() {
        {
            protocol::CommandWriter writer( "test" );

            writer.Write( "name", "Main Lobby/AFK|X\\Y" );

            writer.Write( "count", 42 );

            writer.Write( "enabled", true );

            writer.Write( "empty" );

            const auto serialized = writer.Take();

            const auto expected = Bytes( "test "
                                         "name=Main\\sLobby\\/AFK\\pX\\\\Y "
                                         "count=42 "
                                         "enabled=1 "
                                         "empty" );

            ExpectEqual( serialized, expected, "Command serialization is incorrect" );
        }

        {
            protocol::CommandWriter writer( "channellist" );

            writer.Write( "cid", 1 );

            writer.Write( "channel_name", "Main Lobby" );

            writer.NextRow();

            writer.Write( "cid", 2 );

            writer.Write( "channel_name", "Gaming" );

            const auto serialized = writer.Take();

            const auto expected = Bytes( "channellist "
                                         "cid=1 "
                                         "channel_name=Main\\sLobby"
                                         "|"
                                         "cid=2 "
                                         "channel_name=Gaming" );

            ExpectEqual( serialized, expected, "Multi-row command serialization is incorrect" );

            const protocol::Command parsed = protocol::CommandParser::Parse( std::span<const std::byte>( serialized ) );

            ExpectEqual( parsed.Name(), std::string_view( "channellist" ), "Round-trip command name is incorrect" );

            ExpectEqual( parsed.Rows().size(), std::size_t { 2 }, "Round-trip row count is incorrect" );

            ExpectEqual( parsed.Rows()[0].Require( "channel_name" ),
                         std::string_view( "Main Lobby" ),
                         "First round-trip row is incorrect" );

            ExpectEqual( parsed.Rows()[1].Require( "channel_name" ),
                         std::string_view( "Gaming" ),
                         "Second round-trip row is incorrect" );
        }

        {
            protocol::CommandWriter writer( "test" );

            writer.Write( "bare" );

            const protocol::Command parsed = protocol::CommandParser::Parse( std::span<const std::byte>( writer.Take() ) );

            const auto value = parsed.Rows()[0].Find( "bare" );

            Expect( value.has_value(), "Bare command parameter disappeared" );

            Expect( value->empty(), "Bare command parameter should have an empty value" );
        }

        {
            protocol::CommandWriter writer( "test" );

            const std::string value( "a\0b", 3 );

            ExpectThrows<std::runtime_error>(
                [&writer, &value]() {
                    writer.Write( "value", value );
                },
                "Null byte in command value did not fail" );
        }

        ExpectThrows<std::runtime_error>(
            []() {
                protocol::CommandWriter writer( "" );

                (void)writer;
            },
            "Empty command name did not fail" );

        ExpectThrows<std::runtime_error>(
            []() {
                protocol::CommandWriter writer( "invalid command" );

                (void)writer;
            },
            "Command name containing a space did not fail" );

        {
            protocol::CommandWriter writer( "test" );

            writer.Write( "value", 1 );

            writer.NextRow();

            ExpectThrows<std::runtime_error>(
                [&writer]() {
                    (void)writer.Take();
                },
                "Trailing empty command row did not fail" );
        }

        {
            protocol::CommandWriter writer( "test" );

            ExpectThrows<std::runtime_error>(
                [&writer]() {
                    writer.NextRow();
                },
                "Empty command row did not fail" );
        }
    }

} // namespace ts::test
