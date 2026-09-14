#include "test_support.hpp"

#include <cstdint>
#include <protocol/session/event.hpp>
#include <string>
#include <vector>

namespace ts::test {

    namespace {

        /* Records which alternative Visit routed each event to. */
        std::string Dispatch( const protocol::SessionEvent& event ) {
            std::string seen;

            protocol::Visit(
                event,
                [&seen]( const protocol::TextMessageEvent& value ) {
                    seen = "text:" + value.message.text;
                },
                [&seen]( const protocol::ClientPresenceEvent& value ) {
                    seen = "presence:" + value.clientName;
                },
                [&seen]( const protocol::CommandErrorEvent& value ) {
                    seen = "error:" + std::to_string( value.id );
                },
                [&seen]( const protocol::VoiceEvent& value ) {
                    seen = "voice:" + std::to_string( value.frame.clientId );
                },
                [&seen]( const protocol::PokeEvent& value ) {
                    seen = "poke:" + value.invokerName;
                } );

            return seen;
        }

    } // namespace

    void RunSessionEventTests() {
        protocol::TextMessageEvent text;
        text.message.text = "hello";
        ExpectEqual( Dispatch( text ), std::string( "text:hello" ), "Visit did not dispatch a TextMessageEvent" );

        protocol::ClientPresenceEvent presence;
        presence.clientName = "alice";
        ExpectEqual( Dispatch( presence ), std::string( "presence:alice" ), "Visit did not dispatch a ClientPresenceEvent" );

        protocol::CommandErrorEvent commandError;
        commandError.id = 512;
        ExpectEqual( Dispatch( commandError ), std::string( "error:512" ), "Visit did not dispatch a CommandErrorEvent" );

        protocol::VoiceEvent voice;
        voice.frame.clientId = 7;
        ExpectEqual( Dispatch( voice ), std::string( "voice:7" ), "Visit did not dispatch a VoiceEvent" );

        protocol::PokeEvent poke;
        poke.invokerName = "bob";
        ExpectEqual( Dispatch( poke ), std::string( "poke:bob" ), "Visit did not dispatch a PokeEvent" );

        /*
         * A generic handler must absorb every alternative, which is what lets
         * a caller opt out of exhaustiveness when it only cares about one
         * event kind.
         */
        std::size_t generic = 0;

        for ( const protocol::SessionEvent& event :
              std::vector<protocol::SessionEvent> { text, presence, commandError, voice, poke } ) {
            protocol::Visit( event, [&generic]( const auto& ) {
                ++generic;
            } );
        }

        ExpectEqual( generic, std::size_t { 5 }, "A generic handler did not absorb every event alternative" );

        /* A specific handler must still win over a generic fallback. */
        std::string specific;

        protocol::Visit(
            protocol::SessionEvent { poke },
            [&specific]( const protocol::PokeEvent& value ) {
                specific = value.invokerName;
            },
            [&specific]( const auto& ) {
                specific = "fallback";
            } );

        ExpectEqual( specific, std::string( "bob" ), "A generic fallback shadowed a specific handler" );
    }

} // namespace ts::test
