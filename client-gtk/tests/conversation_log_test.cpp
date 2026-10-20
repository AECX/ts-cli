#include "test_support.hpp"

#include <client/gtk/conversation_log.hpp>
#include <cstddef>
#include <string>

namespace ts::client::gtk::test {

    void RunConversationLogTests() {
        ConversationLog log;
        ExpectEqual( log.Active(), ServerConversation(), "Log does not start on the server conversation" );

        Expect( log.Append( ServerConversation(),
                            FeedLine { .kind = FeedLineKind::Info, .time = {}, .sender = {}, .text = "hello" } ),
                "Line for the active conversation was not reported as active" );
        ExpectEqual( log.Unread( ServerConversation() ), std::size_t { 0 }, "Active conversation counted unread lines" );

        const ConversationKey lobby = ChannelConversation( 7 );
        const ConversationKey peer = ClientConversation( 11 );
        Expect( lobby != peer, "Channel and client conversations with related IDs compare equal" );
        Expect( ChannelConversation( 11 ) != ClientConversation( 11 ), "Conversation kind is ignored by comparison" );

        Expect( !log.Append( lobby, FeedLine { .kind = FeedLineKind::Message, .time = {}, .sender = "a", .text = "one" } ),
                "Line for an inactive conversation was reported as active" );
        (void)log.Append( lobby, FeedLine { .kind = FeedLineKind::Message, .time = {}, .sender = "b", .text = "two" } );
        ExpectEqual( log.Unread( lobby ), std::size_t { 2 }, "Inactive conversation unread count is wrong" );
        ExpectEqual( log.Unread( peer ), std::size_t { 0 }, "Untouched conversation has unread lines" );

        log.SetActive( lobby );
        ExpectEqual( log.Unread( lobby ), std::size_t { 0 }, "Activating a conversation did not clear its unread count" );
        ExpectEqual( log.Lines( lobby ).size(), std::size_t { 2 }, "Conversation history is incomplete" );
        ExpectEqual( log.Lines( lobby ).back().text, std::string( "two" ), "Conversation history order changed" );
        ExpectEqual( log.Lines( ServerConversation() ).size(), std::size_t { 1 }, "Conversations share history" );

        for ( std::size_t index = 0; index < ConversationLog::MaxLines() + 5; ++index ) {
            (void)log.Append(
                peer,
                FeedLine { .kind = FeedLineKind::Message, .time = {}, .sender = {}, .text = std::to_string( index ) } );
        }
        ExpectEqual( log.Lines( peer ).size(), ConversationLog::MaxLines(), "Conversation history is unbounded" );
        ExpectEqual( log.Lines( peer ).front().text, std::string( "5" ), "History trimming dropped the wrong lines" );

        log.Clear( lobby );
        Expect( log.Lines( lobby ).empty(), "Clearing a conversation kept its lines" );
        ExpectEqual( log.Active(), lobby, "Clearing the active conversation changed the selection" );
    }

} // namespace ts::client::gtk::test
