#include <algorithm>
#include <array>
#include <audio/audio_types.hpp>
#include <chrono>
#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/gtk/event_router.hpp>
#include <client/gtk/main_window.hpp>
#include <client/runtime/action.hpp>
#include <client/runtime/event.hpp>
#include <config/config.hpp>
#include <exception>
#include <glibmm/datetime.h>
#include <glibmm/main.h>
#include <optional>
#include <protocol/message/text_message.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ts::client::gtk {

    namespace {

        constexpr unsigned int TickMilliseconds = 30;

        constexpr std::array<std::string_view, 25> HelpLines = {
            "Text without a leading / goes to the conversation selected in the tree:",
            "  the server, your current channel, or a client (private message).",
            "Double-click a channel to join it.",
            "Commands:",
            "  /pm <client> <text>             private message; clients only",
            "  /message <client> <text>        alias for /pm",
            "  /r <text>                       reply to the last received private message",
            "  /join <channel>                 move to a channel",
            "  /nick <new name>                change nickname",
            "  /list [channel]                 print the channel/client tree",
            "  /user <client>                  show persistent local settings for a client",
            "  /user <client> volume <dB|%>    set that client's playback volume",
            "  /user <client> volume reset     restore normal playback volume",
            "  /user <client> mute|unmute      locally mute or unmute that client",
            "  /audio devices                  list input/output devices",
            "  /audio status                   show audio status",
            "  /audio input <default|id|name>  select microphone",
            "  /audio output <default|id|name> select playback device",
            "  /audio filter [none|name]       list or select capture filters",
            "  /audio threshold <-100..0>      set microphone activation threshold in dBFS",
            "  /mute, /unmute                  mute or unmute microphone transmission",
            "  /clear                          clear this conversation",
            "  /help                           show this help",
            "  /quit                           disconnect and exit",
            "Quote client names containing spaces. Use #<client-id> to disambiguate clients." };

        [[nodiscard]] std::string CurrentTime() {
            return Glib::DateTime::create_now_local().format( "%H:%M" );
        }

        [[nodiscard]] const protocol::Client* FindClient( const ServerSnapshot& snapshot, std::uint64_t clientId ) {
            const auto client =
                std::find_if( snapshot.clients.begin(), snapshot.clients.end(), [clientId]( const protocol::Client& value ) {
                    return value.id == clientId;
                } );
            return client == snapshot.clients.end() ? nullptr : &*client;
        }

        [[nodiscard]] const protocol::Channel* FindChannel( const ServerSnapshot& snapshot, std::uint64_t channelId ) {
            const auto channel =
                std::find_if( snapshot.channels.begin(), snapshot.channels.end(), [channelId]( const ChannelSnapshot& value ) {
                    return value.channel.id == channelId;
                } );
            return channel == snapshot.channels.end() ? nullptr : &channel->channel;
        }

    } // namespace

    MainWindow::IconToggle::IconToggle() {
        content.append( icon );
        content.append( label );
        button.set_child( content );
    }

    void MainWindow::IconToggle::Show( const Glib::ustring& iconName, const Glib::ustring& text ) {
        icon.set_from_icon_name( iconName );
        label.set_text( text );
    }

    MainWindow::MainWindow( audio::AudioEngine& audio,
                            config::Config& config,
                            const config::Paths& paths,
                            config::UserConfigStore& userConfigs,
                            NetworkWake& wake ):
        m_Audio( audio ), m_Config( config ), m_Paths( paths ), m_UserConfigs( userConfigs ), m_Wake( wake ),
        m_Options(
            audio,
            config,
            paths.IdentityFile(),
            [this] {
                PersistAudioSettings();
            },
            [this]( const std::string& nickname ) {
                if ( m_State != ConnectionState::Connected ) {
                    return false;
                }

                /* Like /nick; a rejected name is reported in the feed. */
                m_Session->Actions().Push( ChangeNicknameAction { .nickname = nickname } );
                return true;
            } ) {
        set_title( "ts-gtk" );
        set_default_size( 700, 760 );

        /* Closing ends the session through on_hide() and the application's hide handler; GTK would destroy it instead. */
        set_hide_on_close( true );

        m_TopBar.append( m_Microphone.button );
        m_TopBar.append( m_Speakers.button );
        m_OptionsButton.set_tooltip_text(
            "Audio devices, RNNoise, activation threshold, microphone test, nickname and identity" );
        m_TopBar.append( m_OptionsButton );
        m_Options.set_transient_for( *this );
        m_Server.set_hexpand( true );
        m_Server.set_width_chars( 18 );
        m_Server.set_placeholder_text( "Server address" );
        m_Server.set_tooltip_text( "host, host:port or IP address of the server, e.g. voice.example.net:9987" );
        m_Server.set_text( DisplayText( config.LastServer() ) );
        m_TopBar.append( m_Server );
        m_TopBar.append( m_ConnectButton );

        m_TreeScroll.set_policy( Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC );
        m_TreeScroll.set_has_frame( true );
        m_TreeScroll.set_child( m_Tree );

        m_ConversationTitle.set_xalign( 0.0F );
        m_ConversationTitle.set_ellipsize( Pango::EllipsizeMode::END );
        m_Feed.set_vexpand( true );
        m_FeedBox.append( m_ConversationTitle );
        m_FeedBox.append( m_Feed );

        m_Split.set_start_child( m_TreeScroll );
        m_Split.set_end_child( m_FeedBox );
        m_Split.set_shrink_start_child( false );
        m_Split.set_shrink_end_child( false );
        m_Split.set_position( 360 );
        m_Split.set_vexpand( true );

        m_Layout.set_margin( 6 );
        m_Layout.append( m_TopBar );
        m_Layout.append( m_Split );
        m_Layout.append( m_Input );
        set_child( m_Layout );

        m_Microphone.button.signal_toggled().connect( sigc::mem_fun( *this, &MainWindow::MicrophoneToggled ) );
        m_Speakers.button.signal_toggled().connect( sigc::mem_fun( *this, &MainWindow::SpeakersToggled ) );
        m_OptionsButton.signal_clicked().connect( [this] {
            m_Options.present();
            SyncAudioControls();
        } );
        m_Server.signal_activate().connect( sigc::mem_fun( *this, &MainWindow::ConnectClicked ) );
        m_ConnectButton.signal_clicked().connect( sigc::mem_fun( *this, &MainWindow::ConnectClicked ) );
        m_UserAudio.OnChanged( [this]( std::uint16_t clientId, const config::UserConfig& settings ) {
            if ( m_State == ConnectionState::Connected ) {
                m_Session->Actions().Push( SetUserAudioAction { .clientId = clientId, .settings = settings } );
            }
        } );
        m_Input.signal_activate().connect( sigc::mem_fun( *this, &MainWindow::SubmitInput ) );

        m_Tree.OnSelected( [this]( const ConversationKey& key ) {
            m_UserSelected = true;
            SelectConversation( key );
        } );
        m_Tree.OnActivated( [this]( const ConversationKey& key ) {
            JoinChannel( key );
        } );
        m_Tree.OnContextMenu( [this]( const ConversationKey& key, const Gdk::Rectangle& area ) {
            ShowUserAudio( key, area );
        } );

        SetConnectionState( ConnectionState::Disconnected );
        SelectConversation( ServerConversation() );

        m_Ticker = Glib::signal_timeout().connect( sigc::mem_fun( *this, &MainWindow::Tick ), TickMilliseconds );
    }

    MainWindow::~MainWindow() {
        m_Ticker.disconnect();
    }

    void MainWindow::Connect( const std::string& endpoint ) {
        if ( m_Session ) {
            return;
        }

        m_Server.set_text( DisplayText( endpoint ) );

        if ( endpoint.empty() ) {
            PrintError( "Enter the address of the server to connect to" );
            return;
        }

        /* A new server: conversations, unread counts and the tree start over. */
        m_Log = ConversationLog {};
        m_Snapshot = ServerSnapshot {};
        m_UserSelected = false;
        m_Tree.Clear();
        SelectConversation( ServerConversation() );

        try {
            config::LocalIdentity identity = config::IdentityStore::Load( m_Paths.IdentityFile() );
            m_Session = std::make_unique<Session>( protocol::ConnectionOptions { .endpoint = endpoint,
                                                                                 .profile = m_Config.Profile(),
                                                                                 .identity = std::move( identity.identity ),
                                                                                 .identityKeyOffset = identity.keyOffset },
                                                   m_Audio,
                                                   m_UserConfigs,
                                                   m_Wake );
        } catch ( const std::exception& exception ) {
            PrintError( "Could not connect: " + std::string( exception.what() ) );
            return;
        }

        m_Endpoint = endpoint;
        SetConnectionState( ConnectionState::Connecting );
        PrintInfo( "Connecting to " + endpoint + "..." );
    }

    void MainWindow::Shutdown() {
        m_Ticker.disconnect();

        if ( !m_Session ) {
            return;
        }

        m_Session->RequestStop();

        if ( m_Commands ) {
            try {
                m_Session->Join();
            } catch ( const std::exception& ) {
                /* Exiting anyway. */
            }
        }

        m_Commands.reset();
        m_Session.reset();
    }

    void MainWindow::on_hide() {
        /* Restores the microphone state the options muted, before the session ends. */
        m_Options.set_visible( false );
        Gtk::Window::on_hide();
    }

    void MainWindow::PrintError( std::string_view message ) {
        Notify( audio::NotificationType::Failure );
        AppendLine( std::nullopt,
                    FeedLine { .kind = FeedLineKind::Error, .time = {}, .sender = {}, .text = std::string( message ) } );
    }

    void MainWindow::PrintInfo( std::string_view message ) {
        AppendLine( std::nullopt,
                    FeedLine { .kind = FeedLineKind::Info, .time = {}, .sender = {}, .text = std::string( message ) } );
    }

    void MainWindow::PrintHelp() {
        for ( const std::string_view line : HelpLines ) {
            PrintInfo( line );
        }
    }

    void MainWindow::Clear() {
        m_Log.Clear( m_Log.Active() );
        m_Feed.Show( m_Log.Lines( m_Log.Active() ) );
    }

    bool MainWindow::Tick() {
        PollSession();

        if ( m_TreeDirty && m_State == ConnectionState::Connected ) {
            m_TreeDirty = false;
            m_Tree.Update( m_Snapshot, m_Log );
            UpdateTitles();
        }

        SyncAudioControls();
        return true;
    }

    void MainWindow::PollSession() {
        if ( !m_Session ) {
            return;
        }

        if ( !m_Commands ) {
            switch ( m_Session->Poll() ) {
                case Session::State::Connecting:
                    return;

                case Session::State::Connected:
                    SessionStarted();
                    break;

                case Session::State::Failed:
                    if ( m_State == ConnectionState::Disconnecting ) {
                        PrintInfo( "Connecting cancelled" );
                    } else {
                        PrintError( "Could not connect to " + m_Endpoint + ": " + m_Session->Error() );
                    }

                    m_Session.reset();
                    SetConnectionState( ConnectionState::Disconnected );
                    return;
            }
        }

        DrainEvents();

        if ( m_Session->Ended() ) {
            /* The runtime may have queued its last events after the drain above. */
            DrainEvents();
            EndSession();
        }
    }

    void MainWindow::DrainEvents() {
        RuntimeEvent event;

        while ( m_Session->Events().WaitPop( event, std::chrono::milliseconds( 0 ) ) ) {
            m_Commands->Observe( event );

            if ( auto* snapshot = std::get_if<ServerSnapshotEvent>( &event ); snapshot != nullptr ) {
                m_Snapshot = std::move( snapshot->snapshot );
                m_TreeDirty = true;

                if ( !m_UserSelected && m_Snapshot.currentChannelId != 0 ) {
                    SelectConversation( ChannelConversation( m_Snapshot.currentChannelId ) );
                }
                continue;
            }

            if ( auto* talk = std::get_if<TalkStateEvent>( &event ); talk != nullptr ) {
                m_Tree.SetTalking( std::move( talk->talkingClientIds ) );
                continue;
            }

            const RoutedEvent routed = RouteEvent( event, m_Snapshot );

            for ( const RoutedLine& line : routed.lines ) {
                AppendLine( line.conversation, line.line );
            }

            if ( routed.notification ) {
                Notify( *routed.notification );
            }
        }
    }

    void MainWindow::SessionStarted() {
        m_Commands.emplace(
            m_Session->Actions(),
            *this,
            [this]( audio::NotificationType type ) {
                Notify( type );
            },
            &m_Audio,
            [this]( const config::ConfigMutator& mutator ) {
                m_Config.Update( mutator );
            } );

        /* Cancelled while the connection was being established. */
        const bool cancelled = m_State == ConnectionState::Disconnecting;
        SetConnectionState( ConnectionState::Connected );

        if ( cancelled ) {
            m_Session->RequestStop();
            SetConnectionState( ConnectionState::Disconnecting );
            return;
        }

        try {
            m_Config.Update( [this]( config::Config& config ) {
                config.SetLastServer( m_Endpoint );
            } );
        } catch ( const std::exception& exception ) {
            PrintError( std::string( "could not remember the server: " ) + exception.what() );
        }

        PrintInfo( "Connected. Type /help for commands" );
        m_Input.grab_focus();
    }

    void MainWindow::EndSession() {
        try {
            m_Session->Join();
        } catch ( const std::exception& exception ) {
            PrintError( exception.what() );
        }

        m_Commands.reset();
        m_Session.reset();

        try {
            /* Every session starts muted, like ts-cli. */
            m_Audio.SetTransmitEnabled( false );
        } catch ( const std::exception& ) {
            /* Audio is unavailable; nothing is transmitted either way. */
        }

        if ( m_State == ConnectionState::Disconnecting ) {
            /* The runtime only reports disconnects the user did not ask for. */
            PrintInfo( "Disconnected" );
        }

        /* The last snapshot stays for the conversation titles until the next connect. */
        m_UserAudio.popdown();
        m_Tree.Clear();
        SetConnectionState( ConnectionState::Disconnected );
    }

    void MainWindow::ConnectClicked() {
        switch ( m_State ) {
            case ConnectionState::Disconnected:
                Connect( m_Server.get_text() );
                break;

            case ConnectionState::Connecting:
                /* Takes until the attempt gives up; connecting can't be interrupted. */
                m_Session->RequestStop();
                SetConnectionState( ConnectionState::Disconnecting );
                m_ConnectButton.set_label( "Cancelling..." );
                break;

            case ConnectionState::Connected:
                m_Session->RequestStop();
                SetConnectionState( ConnectionState::Disconnecting );
                break;

            case ConnectionState::Disconnecting:
                break;
        }
    }

    void MainWindow::SetConnectionState( ConnectionState state ) {
        m_State = state;

        const bool disconnected = state == ConnectionState::Disconnected;

        /* GTK expects focus to leave an entry (its inner text widget, really) before it is disabled. */
        if ( const Gtk::Widget* focus = get_focus(); focus != nullptr ) {
            if ( ( !disconnected && focus->is_ancestor( m_Server ) ) ||
                 ( state != ConnectionState::Connected && focus->is_ancestor( m_Input ) ) ) {
                unset_focus();
            }
        }

        m_Server.set_sensitive( disconnected );
        m_Input.set_sensitive( state == ConnectionState::Connected );
        m_ConnectButton.set_sensitive( state != ConnectionState::Disconnecting );
        m_ConnectButton.remove_css_class( "suggested-action" );
        m_ConnectButton.remove_css_class( "destructive-action" );

        switch ( state ) {
            case ConnectionState::Disconnected:
                m_ConnectButton.set_label( "Connect" );
                m_ConnectButton.add_css_class( "suggested-action" );
                m_Server.grab_focus();
                set_title( "ts-gtk" );
                break;

            case ConnectionState::Connecting:
                m_ConnectButton.set_label( "Cancel" );
                set_title( DisplayText( "ts-gtk: connecting to " + m_Endpoint ) );
                break;

            case ConnectionState::Connected:
                m_ConnectButton.set_label( "Disconnect" );
                m_ConnectButton.add_css_class( "destructive-action" );
                break;

            case ConnectionState::Disconnecting:
                m_ConnectButton.set_label( "Disconnecting..." );
                break;
        }

        UpdateTitles();
        SyncAudioControls();
    }

    void MainWindow::AppendLine( std::optional<ConversationKey> conversation, FeedLine line ) {
        line.time = CurrentTime();

        if ( m_Log.Append( conversation.value_or( m_Log.Active() ), line ) ) {
            m_Feed.Append( line );
        } else {
            m_TreeDirty = true;
        }
    }

    void MainWindow::SelectConversation( const ConversationKey& key ) {
        const bool unread = m_Log.Unread( key ) != 0;

        m_Log.SetActive( key );
        m_Feed.Show( m_Log.Lines( key ) );
        UpdateTitles();

        /* Rebuilt on the next tick, not from inside the tree's own selection handler. */
        m_TreeDirty = m_TreeDirty || unread || !m_UserSelected;
    }

    void MainWindow::JoinChannel( const ConversationKey& key ) {
        if ( key.kind != ConversationKind::Channel || key.id == m_Snapshot.currentChannelId ||
             m_State != ConnectionState::Connected ) {
            return;
        }

        m_Session->Actions().Push( MoveToChannelAction { .channelId = key.id } );
    }

    void MainWindow::ShowUserAudio( const ConversationKey& key, const Gdk::Rectangle& area ) {
        if ( key.kind != ConversationKind::Client || key.id == m_Snapshot.clientId || m_State != ConnectionState::Connected ) {
            return;
        }

        const protocol::Client* client = FindClient( m_Snapshot, key.id );

        if ( client == nullptr || !client->detailsKnown || client->uniqueId.empty() ) {
            return;
        }

        config::UserConfig settings;

        try {
            settings = m_UserConfigs.Load( client->uniqueId );
        } catch ( const std::exception& exception ) {
            PrintError( "user settings for " + client->nickname + ": " + exception.what() );
        }

        m_UserAudio.set_pointing_to( area );
        m_UserAudio.Present( client->id, client->nickname, settings );
    }

    void MainWindow::UpdateTitles() {
        const std::string title = ConversationTitle( m_Log.Active() );
        m_ConversationTitle.set_markup( "<b>" + DisplayMarkup( title ) + "</b>" );
        m_Input.set_placeholder_text( m_State == ConnectionState::Connected
                                          ? DisplayText( "Message " + title + " (/help for commands)" )
                                          : Glib::ustring( "Not connected" ) );

        const protocol::Client* self = FindClient( m_Snapshot, m_Snapshot.clientId );
        std::string status = self == nullptr ? std::string {} : self->nickname;

        if ( m_State == ConnectionState::Connected && !m_Snapshot.serverName.empty() ) {
            status += status.empty() ? m_Snapshot.serverName : " @ " + m_Snapshot.serverName;
            set_title( DisplayText( "ts-gtk: " + status ) );
        }
    }

    std::string MainWindow::ConversationTitle( const ConversationKey& key ) const {
        switch ( key.kind ) {
            case ConversationKind::Server:
                return "server chat";

            case ConversationKind::Channel: {
                const protocol::Channel* channel = FindChannel( m_Snapshot, key.id );
                const std::string name = channel == nullptr ? "channel " + std::to_string( key.id ) : channel->name;

                if ( key.id != m_Snapshot.currentChannelId ) {
                    return name + " (double-click to join)";
                }

                return name;
            }

            case ConversationKind::Client: {
                const protocol::Client* client = FindClient( m_Snapshot, key.id );

                if ( client == nullptr ) {
                    return "client " + std::to_string( key.id ) + " (left)";
                }

                return client->nickname + ( key.id == m_Snapshot.clientId ? " (you)" : "" );
            }
        }

        return {};
    }

    void MainWindow::SubmitInput() {
        const std::string text = m_Input.get_text();

        if ( text.empty() || !m_Commands ) {
            return;
        }

        m_Input.set_text( "" );

        if ( text.front() == '/' ) {
            if ( !m_Commands->HandleLine( text ) ) {
                close();
            }
            return;
        }

        SendText( text );
    }

    void MainWindow::SendText( const std::string& text ) {
        const ConversationKey& key = m_Log.Active();

        switch ( key.kind ) {
            case ConversationKind::Server:
                m_Session->Actions().Push( SendTextMessageAction {
                    .target = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Server, .id = 0 },
                    .text = text } );
                break;

            case ConversationKind::Channel:
                if ( key.id != m_Snapshot.currentChannelId ) {
                    PrintError( "You can only write to your current channel; double-click this channel to join it" );
                    return;
                }

                m_Session->Actions().Push( SendTextMessageAction {
                    .target = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Channel, .id = key.id },
                    .text = text } );
                break;

            case ConversationKind::Client:
                if ( key.id == m_Snapshot.clientId ) {
                    PrintError( "You cannot send a private message to yourself" );
                    return;
                }

                if ( FindClient( m_Snapshot, key.id ) == nullptr ) {
                    PrintError( "That client is no longer connected" );
                    return;
                }

                m_Session->Actions().Push( SendTextMessageAction {
                    .target = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = key.id },
                    .text = text } );
                break;
        }
    }

    void MainWindow::SyncAudioControls() {
        const audio::AudioStatus status = m_Audio.Status();
        const bool microphoneMuted = !status.transmitEnabled;
        const bool settingsOpen = m_Options.get_visible();
        const bool connected = m_State == ConnectionState::Connected;

        m_SyncingControls = true;

        m_Microphone.button.set_active( microphoneMuted );
        m_Microphone.Show( microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic",
                           microphoneMuted ? "Microphone muted" : "Microphone on" );
        m_Microphone.button.set_sensitive( status.available && connected && !settingsOpen );

        if ( !status.available ) {
            m_Microphone.button.set_tooltip_text( DisplayText( "Audio unavailable: " + status.error ) );
        } else if ( !connected ) {
            m_Microphone.button.set_tooltip_text( "Not connected" );
        } else if ( settingsOpen ) {
            m_Microphone.button.set_tooltip_text( "Muted while the options are open" );
        } else {
            m_Microphone.button.set_tooltip_text( "Mute or unmute microphone transmission" );
        }

        m_Speakers.button.set_active( status.playbackMuted );
        m_Speakers.Show( status.playbackMuted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic",
                         status.playbackMuted ? "Speakers muted" : "Speakers on" );
        m_Speakers.button.set_tooltip_text( "Mute or unmute everyone's voice (notification sounds still play)" );

        m_SyncingControls = false;
    }

    void MainWindow::MicrophoneToggled() {
        if ( m_SyncingControls ) {
            return;
        }

        const bool muted = m_Microphone.button.get_active();

        try {
            m_Audio.SetTransmitEnabled( !muted );
            Notify( muted ? audio::NotificationType::Failure : audio::NotificationType::Success );
        } catch ( const std::exception& exception ) {
            PrintError( exception.what() );
        }

        SyncAudioControls();
    }

    void MainWindow::SpeakersToggled() {
        if ( m_SyncingControls ) {
            return;
        }

        const bool muted = m_Speakers.button.get_active();
        m_Audio.SetPlaybackMuted( muted );
        Notify( muted ? audio::NotificationType::Failure : audio::NotificationType::Success );
        SyncAudioControls();
    }

    void MainWindow::PersistAudioSettings() {
        try {
            m_Config.Update( [settings = m_Audio.Settings()]( config::Config& config ) {
                config.SetAudioSettings( settings );
            } );
        } catch ( const std::exception& exception ) {
            PrintError( std::string( "could not save audio settings: " ) + exception.what() );
        }
    }

    void MainWindow::Notify( audio::NotificationType type ) {
        m_Audio.Notify( type );
    }

} // namespace ts::client::gtk
