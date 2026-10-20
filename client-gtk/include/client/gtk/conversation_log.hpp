#ifndef TS_CLIENT_GTK_CONVERSATION_LOG_HPP
#define TS_CLIENT_GTK_CONVERSATION_LOG_HPP

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ts::client::gtk {

    enum class ConversationKind { Server, Channel, Client };

    /*
     * One text feed: the server chat, a channel chat (by channel ID) or a
     * private conversation (by client ID).
     */
    struct ConversationKey {
        ConversationKind kind = ConversationKind::Server;
        std::uint64_t id = 0;

        auto operator<=>( const ConversationKey& ) const = default;
    };

    [[nodiscard]] ConversationKey ServerConversation();
    [[nodiscard]] ConversationKey ChannelConversation( std::uint64_t channelId );
    [[nodiscard]] ConversationKey ClientConversation( std::uint16_t clientId );

    enum class FeedLineKind { Message, Info, Error };

    struct FeedLine {
        FeedLineKind kind = FeedLineKind::Info;
        std::string time;
        std::string sender;
        std::string text;
    };

    /*
     * Bounded history per conversation plus an unread counter for every
     * conversation that is not the active (displayed) one.
     */
    class ConversationLog {
      public:
        /* Returns true when the line went to the active conversation. */
        bool Append( const ConversationKey& key, FeedLine line );
        void Clear( const ConversationKey& key );

        void SetActive( const ConversationKey& key );
        [[nodiscard]] const ConversationKey& Active() const;

        [[nodiscard]] const std::vector<FeedLine>& Lines( const ConversationKey& key ) const;
        [[nodiscard]] std::size_t Unread( const ConversationKey& key ) const;

        [[nodiscard]] static std::size_t MaxLines();

      private:
        struct Conversation {
            std::vector<FeedLine> lines;
            std::size_t unread = 0;
        };

        std::map<ConversationKey, Conversation> m_Conversations;
        ConversationKey m_Active;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_CONVERSATION_LOG_HPP
