#ifndef TS_CLIENT_GTK_MAIN_WINDOW_HPP
#define TS_CLIENT_GTK_MAIN_WINDOW_HPP

#include "channel_tree.hpp"
#include "conversation_log.hpp"
#include "feed_view.hpp"
#include "options_window.hpp"
#include "session.hpp"
#include "user_audio_popover.hpp"

#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <client/runtime/server_snapshot.hpp>
#include <config/config.hpp>
#include <config/paths.hpp>
#include <config/user_config.hpp>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ts::client::gtk {

    /*
     * Top to bottom: audio controls and the server to connect to, the live
     * channel/client tree, the feed of the selected conversation and the
     * input line. Runs entirely on the GTK thread and talks to the current
     * session's network thread only through its action/event queues. The
     * input line only sends chat; everything else is done through the UI.
     */
    class MainWindow: public Gtk::Window {
      public:
        MainWindow( audio::AudioEngine& audio,
                    config::Config& config,
                    const config::Paths& paths,
                    config::UserConfigStore& userConfigs,
                    NetworkWake& wake );

        ~MainWindow() override;

        /* Starts connecting to `endpoint` unless a session is already running. */
        void Connect( const std::string& endpoint );

        /* Disconnects and waits for the session to end; call before destroying the window. */
        void Shutdown();

        MainWindow( const MainWindow& ) = delete;
        MainWindow& operator=( const MainWindow& ) = delete;

      protected:
        void on_hide() override;

      private:
        /* A toggle button showing an icon next to its label. */
        struct IconToggle {
            IconToggle();

            void Show( const Glib::ustring& iconName, const Glib::ustring& text );

            Gtk::ToggleButton button;
            Gtk::Box content { Gtk::Orientation::HORIZONTAL, 6 };
            Gtk::Image icon;
            Gtk::Label label;
        };

        enum class ConnectionState { Disconnected, Connecting, Connected, Disconnecting };

        bool Tick();
        void PollSession();
        void DrainEvents();
        void SessionStarted();
        void EndSession();
        void ConnectClicked();
        void SetConnectionState( ConnectionState state );

        void PrintError( std::string_view message );
        void PrintInfo( std::string_view message );
        void AppendLine( std::optional<ConversationKey> conversation, FeedLine line );
        void SelectConversation( const ConversationKey& key );
        void JoinChannel( const ConversationKey& key );
        void ShowUserAudio( const ConversationKey& key, const Gdk::Rectangle& area );
        void UpdateTitles();
        [[nodiscard]] std::string ConversationTitle( const ConversationKey& key ) const;

        void SubmitInput();
        void SendText( const std::string& text );

        void SyncAudioControls();
        void MicrophoneToggled();
        void SpeakersToggled();
        void PersistAudioSettings();
        void Notify( audio::NotificationType type );

        audio::AudioEngine& m_Audio;
        config::Config& m_Config;
        const config::Paths& m_Paths;
        config::UserConfigStore& m_UserConfigs;
        NetworkWake& m_Wake;

        /* Exists only while connecting or connected; started once the connection is established. */
        std::unique_ptr<Session> m_Session;
        bool m_SessionStarted = false;
        ConnectionState m_State = ConnectionState::Disconnected;
        std::string m_Endpoint;

        ServerSnapshot m_Snapshot;
        ConversationLog m_Log;
        bool m_SyncingControls = false;
        bool m_TreeDirty = false;
        bool m_UserSelected = false;

        Gtk::Box m_Layout { Gtk::Orientation::VERTICAL, 6 };
        Gtk::Box m_TopBar { Gtk::Orientation::HORIZONTAL, 6 };
        IconToggle m_Microphone;
        IconToggle m_Speakers;
        Gtk::Button m_OptionsButton { "Options" };
        Gtk::Entry m_Server;
        Gtk::Button m_ConnectButton;
        Gtk::Paned m_Split { Gtk::Orientation::VERTICAL };
        Gtk::ScrolledWindow m_TreeScroll;
        ChannelTree m_Tree;
        UserAudioPopover m_UserAudio { m_Tree };
        Gtk::Box m_FeedBox { Gtk::Orientation::VERTICAL, 4 };
        Gtk::Label m_ConversationTitle;
        FeedView m_Feed;
        Gtk::Entry m_Input;
        OptionsWindow m_Options;
        sigc::connection m_Ticker;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_MAIN_WINDOW_HPP
