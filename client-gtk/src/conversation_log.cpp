#include <client/gtk/conversation_log.hpp>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace ts::client::gtk {

    ConversationKey ServerConversation() {
        return ConversationKey { .kind = ConversationKind::Server, .id = 0 };
    }

    ConversationKey ChannelConversation( std::uint64_t channelId ) {
        return ConversationKey { .kind = ConversationKind::Channel, .id = channelId };
    }

    ConversationKey ClientConversation( std::uint16_t clientId ) {
        return ConversationKey { .kind = ConversationKind::Client, .id = clientId };
    }

    bool ConversationLog::Append( const ConversationKey& key, FeedLine line ) {
        Conversation& conversation = m_Conversations[key];
        conversation.lines.push_back( std::move( line ) );

        if ( conversation.lines.size() > MaxLines() ) {
            const auto excess = static_cast<std::ptrdiff_t>( conversation.lines.size() - MaxLines() );
            conversation.lines.erase( conversation.lines.begin(), std::next( conversation.lines.begin(), excess ) );
        }

        if ( key == m_Active ) {
            return true;
        }

        ++conversation.unread;
        return false;
    }

    void ConversationLog::Clear( const ConversationKey& key ) {
        m_Conversations.erase( key );
    }

    void ConversationLog::SetActive( const ConversationKey& key ) {
        m_Active = key;

        if ( const auto conversation = m_Conversations.find( key ); conversation != m_Conversations.end() ) {
            conversation->second.unread = 0;
        }
    }

    const ConversationKey& ConversationLog::Active() const {
        return m_Active;
    }

    const std::vector<FeedLine>& ConversationLog::Lines( const ConversationKey& key ) const {
        static const std::vector<FeedLine> Empty;

        const auto conversation = m_Conversations.find( key );
        return conversation == m_Conversations.end() ? Empty : conversation->second.lines;
    }

    std::size_t ConversationLog::Unread( const ConversationKey& key ) const {
        const auto conversation = m_Conversations.find( key );
        return conversation == m_Conversations.end() ? 0 : conversation->second.unread;
    }

    std::size_t ConversationLog::MaxLines() {
        return 1000;
    }

} // namespace ts::client::gtk
