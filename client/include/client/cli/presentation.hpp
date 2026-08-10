#ifndef TS_CLIENT_CLI_PRESENTATION_HPP
#define TS_CLIENT_CLI_PRESENTATION_HPP

#include <audio/audio_types.hpp>
#include <client/notification.hpp>
#include <client/runtime/event.hpp>
#include <functional>
#include <iosfwd>
#include <log/terminal_color.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/session/event.hpp>
#include <string>
#include <string_view>

namespace ts::client::cli {

    class Presentation {
      public:
        Presentation( std::ostream& output, bool useColor, NotificationCallback notify );

        void Print( const RuntimeEvent& event );
        void Print( const protocol::SessionEvent& event );
        void Clear();
        void SetCurrentChannel( std::string_view channel );
        void SetCurrentNickname( std::string_view nickname );
        void PrintPrompt();
        void BreakPromptLine();
        void ClearSubmittedInputLine();

        void PrintHelp();
        void PrintError( std::string_view message );
        void PrintInfo( std::string_view message );

      private:
        void PrintTextMessage( const protocol::TextMessageEvent& event );
        void PrintPresence( const protocol::ClientPresenceEvent& event );
        void PrintCommandError( const protocol::CommandErrorEvent& event );

        void WriteSafe( std::string_view value );

        [[nodiscard]] static log::TerminalColor Color( protocol::TextMessageTargetMode targetMode );

        std::ostream& m_Output;
        std::string m_CurrentChannel;
        std::string m_CurrentNickname;
        bool m_UseColor = false;
        NotificationCallback m_Notify;
    };

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_PRESENTATION_HPP
