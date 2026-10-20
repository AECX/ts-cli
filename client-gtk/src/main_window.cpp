#include <algorithm>
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

    MainWindow::MainWindow( ActionQueue& actionQueue,
                            EventQueue& eventQueue,
                            audio::AudioEngine& audio,
                            const config::UserConfigStore& userConfigs,
                            PersistConfigCallback persistConfig ):
        m_ActionQueue( actionQueue ), m_EventQueue( eventQueue ), m_Audio( audio ), m_UserConfigs( userConfigs ),
        m_PersistConfig( std::move( persistConfig ) ), m_AudioSettings( audio, [this] {
            PersistAudioSettings();
        } ) {
        set_title( "ts-gtk" );
        set_default_size( 520, 760 );

        /* Closing ends the session through on_hide() and the application's hide handler; GTK would destroy it instead. */
        set_hide_on_close( true );

        m_Status.set_ellipsize( Pango::EllipsizeMode::END );
        m_Status.set_xalign( 1.0F );
        m_Status.set_hexpand( true );

        m_AudioBar.append( m_Microphone.button );
        m_AudioBar.append( m_Speakers.button );
        m_AudioSettingsButton.set_label( "Options" );
        m_AudioSettingsButton.set_tooltip_text( "Audio settings: devices, RNNoise, activation threshold, microphone test" );
        m_AudioBar.append( m_AudioSettingsButton );
        m_AudioSettings.set_transient_for( *this );
        m_AudioBar.append( m_Status );

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
        m_Layout.append( m_AudioBar );
        m_Layout.append( m_Split );
        m_Layout.append( m_Input );
        set_child( m_Layout );

        m_Microphone.button.signal_toggled().connect( sigc::mem_fun( *this, &MainWindow::MicrophoneToggled ) );
        m_Speakers.button.signal_toggled().connect( sigc::mem_fun( *this, &MainWindow::SpeakersToggled ) );
        m_AudioSettingsButton.signal_clicked().connect( [this] {
            m_AudioSettings.present();
            SyncAudioControls();
        } );
        m_UserAudio.OnChanged( [this]( std::uint16_t clientId, const config::UserConfig& settings ) {
            m_ActionQueue.Push( SetUserAudioAction { .clientId = clientId, .settings = settings } );
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

        SyncAudioControls();
        SelectConversation( ServerConversation() );

        m_Ticker = Glib::signal_timeout().connect( sigc::mem_fun( *this, &MainWindow::Tick ), TickMilliseconds );

        m_Input.grab_focus();
    }

    MainWindow::~MainWindow() {
        m_Ticker.disconnect();
    }

    void MainWindow::on_hide() {
        /* Restores the microphone state the settings window muted, before the session ends. */
        m_AudioSettings.set_visible( false );
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

    bool MainWindow::Tick() {
        RuntimeEvent event;

        while ( m_EventQueue.WaitPop( event, std::chrono::milliseconds( 0 ) ) ) {
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

        if ( m_TreeDirty ) {
            m_TreeDirty = false;
            m_Tree.Update( m_Snapshot, m_Log );
            UpdateTitles();
        }

        SyncAudioControls();

        if ( m_EventQueue.Closed() ) {
            SessionEnded();
            return false;
        }

        return true;
    }

    void MainWindow::SessionEnded() {
        m_SessionEnded = true;
        m_Input.set_sensitive( false );
        m_Microphone.button.set_sensitive( false );
        m_Speakers.button.set_sensitive( false );
        m_AudioSettingsButton.set_sensitive( false );
        m_AudioSettings.set_visible( false );
        m_UserAudio.popdown();
        m_Tree.SetTalking( {} );
        m_Status.set_text( "Disconnected" );
        set_title( "ts-gtk (disconnected)" );
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
        if ( key.kind != ConversationKind::Channel || key.id == m_Snapshot.currentChannelId || m_SessionEnded ) {
            return;
        }

        m_ActionQueue.Push( MoveToChannelAction { .channelId = key.id } );
    }

    void MainWindow::ShowUserAudio( const ConversationKey& key, const Gdk::Rectangle& area ) {
        if ( key.kind != ConversationKind::Client || key.id == m_Snapshot.clientId || m_SessionEnded ) {
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
        m_Input.set_placeholder_text( DisplayText( "Message " + title ) );

        const protocol::Client* self = FindClient( m_Snapshot, m_Snapshot.clientId );
        std::string status = self == nullptr ? std::string {} : self->nickname;

        if ( !m_Snapshot.serverName.empty() ) {
            status += status.empty() ? m_Snapshot.serverName : " @ " + m_Snapshot.serverName;
            set_title( DisplayText( "ts-gtk: " + m_Snapshot.serverName ) );
        }

        if ( !m_SessionEnded ) {
            m_Status.set_text( DisplayText( status ) );
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

        if ( text.empty() || m_SessionEnded ) {
            return;
        }

        m_Input.set_text( "" );
        SendText( text );
    }

    void MainWindow::SendText( const std::string& text ) {
        const ConversationKey& key = m_Log.Active();

        switch ( key.kind ) {
            case ConversationKind::Server:
                m_ActionQueue.Push( SendTextMessageAction {
                    .target = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Server, .id = 0 },
                    .text = text } );
                break;

            case ConversationKind::Channel:
                if ( key.id != m_Snapshot.currentChannelId ) {
                    PrintError( "You can only write to your current channel; double-click this channel to join it" );
                    return;
                }

                m_ActionQueue.Push( SendTextMessageAction {
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

                m_ActionQueue.Push( SendTextMessageAction {
                    .target = protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = key.id },
                    .text = text } );
                break;
        }
    }

    void MainWindow::SyncAudioControls() {
        if ( m_SessionEnded ) {
            return;
        }

        const audio::AudioStatus status = m_Audio.Status();
        const bool microphoneMuted = !status.transmitEnabled;
        const bool settingsOpen = m_AudioSettings.get_visible();

        m_SyncingControls = true;

        m_Microphone.button.set_active( microphoneMuted );
        m_Microphone.Show( microphoneMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic",
                           microphoneMuted ? "Microphone muted" : "Microphone on" );
        m_Microphone.button.set_sensitive( status.available && !settingsOpen );

        if ( !status.available ) {
            m_Microphone.button.set_tooltip_text( DisplayText( "Audio unavailable: " + status.error ) );
        } else if ( settingsOpen ) {
            m_Microphone.button.set_tooltip_text( "Muted while audio settings are open" );
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
        if ( !m_PersistConfig ) {
            return;
        }

        try {
            m_PersistConfig( [settings = m_Audio.Settings()]( config::Config& config ) {
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
