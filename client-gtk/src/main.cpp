#include <audio/audio_engine.hpp>
#include <client/gtk/main_window.hpp>
#include <client/gtk/platform/shutdown_signal.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event_queue.hpp>
#include <client/runtime/network_runtime.hpp>
#include <config/audio_settings.hpp>
#include <config/config.hpp>
#include <config/identity_store.hpp>
#include <config/paths.hpp>
#include <config/setup.hpp>
#include <config/user_config.hpp>
#include <exception>
#include <filesystem>
#include <glibmm/init.h>
#include <glibmm/main.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/application.h>
#include <iostream>
#include <log/logger.hpp>
#include <log/ostream_sink.hpp>
#include <memory>
#include <optional>
#include <protocol/protocol.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace {

    /* Everything one connected session owns, created once the application is activated. */
    struct Session {
        Session( const char* endpoint, ts::log::Logger& logger );

        ts::config::Paths paths;
        ts::config::Config config;
        ts::protocol::Connection connection;
        ts::client::ActionQueue actionQueue;
        ts::client::EventQueue eventQueue;
        ts::audio::AudioEngine audio;
        ts::config::UserConfigStore userConfigStore;

        /* Both need an established connection. */
        std::optional<ts::client::NetworkRuntime> runtime;
        std::optional<ts::client::gtk::MainWindow> window;
    };

    [[nodiscard]] ts::config::Paths DiscoverPaths() {
        ts::config::Paths paths = ts::config::Paths::Discover();
        paths.EnsureDirectories();
        return paths;
    }

    [[nodiscard]] ts::config::Config LoadConfig( const ts::config::Paths& paths, ts::log::Logger& logger ) {
        const std::filesystem::path configPath = paths.ConfigFile();

        if ( std::filesystem::exists( configPath ) ) {
            return ts::config::Config::Load( configPath );
        }

        logger.Info( "client", "no configuration found, writing defaults to " + configPath.string() );
        return ts::config::ConfigSetup::CreateDefault( configPath, paths.IdentityFile() );
    }

    [[nodiscard]] ts::protocol::ConnectionOptions
        ConnectionOptionsFor( const char* endpoint, const ts::config::Config& config, const ts::config::Paths& paths ) {
        ts::config::LocalIdentity localIdentity = ts::config::IdentityStore::Load( paths.IdentityFile() );
        return ts::protocol::ConnectionOptions { .endpoint = endpoint,
                                                 .profile = config.Profile(),
                                                 .identity = std::move( localIdentity.identity ),
                                                 .identityKeyOffset = localIdentity.keyOffset };
    }

    Session::Session( const char* endpoint, ts::log::Logger& logger ):
        paths( DiscoverPaths() ), config( LoadConfig( paths, logger ) ),
        connection( ConnectionOptionsFor( endpoint, config, paths ) ), audio( [this] {
            connection.Wake();
        } ),
        userConfigStore( paths.UsersDirectory() ) {
        logger.Info( "connection", std::string( "connecting to " ) + endpoint );
        connection.Connect();
        logger.Info( "connection", "connected as client " + std::to_string( connection.ClientId() ) );

        ts::config::ApplyAudioSettings( audio, config.AudioSettings(), [&logger]( std::string_view message ) {
            logger.Warning( "audio", std::string( message ) );
        } );

        runtime.emplace( connection, actionQueue, eventQueue, audio, userConfigStore );
        window.emplace( actionQueue, eventQueue, audio, userConfigStore, [this]( const ts::config::ConfigMutator& mutator ) {
            config.Update( mutator );
        } );

        runtime->EnableLiveStateEvents();
        runtime->Start();
    }

} // namespace

int main( int argc, char* argv[] ) {
    if ( argc != 2 ) {
        std::cerr << "Usage: ts-gtk <endpoint>" << std::endl;
        return 1;
    }

    ts::log::OstreamSink sink( std::cerr, false );
    ts::log::Logger logger( sink, ts::log::Level::Info );

    /*
     * glibmm would otherwise switch the global C++ locale to the user's,
     * which config.conf's number formatting and parsing must not follow.
     */
    Glib::set_init_to_users_preferred_locale( false );

    const char* endpoint = argv[1];
    const Glib::RefPtr<Gtk::Application> application = Gtk::Application::create( "", Gio::Application::Flags::NON_UNIQUE );
    std::unique_ptr<Session> session;
    int exitCode = 0;

    /*
     * gtkmm 4 widgets can only be created once the application is
     * activated, and must be gone before run() returns. The application is
     * held until the session has disconnected, or the error dialog closed.
     */
    application->signal_activate().connect( [&] {
        application->hold();

        try {
            session = std::make_unique<Session>( endpoint, logger );
        } catch ( const std::exception& exception ) {
            logger.Error( "client", exception.what() );
            exitCode = 1;

            const Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create( "ts-gtk could not start" );
            dialog->set_detail( exception.what() );
            dialog->set_buttons( { "Close" } );
            dialog->choose( [application, dialog]( const Glib::RefPtr<Gio::AsyncResult>& result ) {
                try {
                    (void)dialog->choose_finish( result );
                } catch ( const Glib::Error& ) {
                    /* Dismissed without a button; closing either way. */
                }
                application->release();
            } );
            return;
        }

        ts::client::gtk::MainWindow& window = *session->window;

        ts::client::gtk::platform::InstallShutdownHandler( [&session] {
            if ( session ) {
                session->window->close();
            }
        } );

        window.signal_hide().connect( [&] {
            /* Deferred: the window must not be destroyed from inside its own hide handler. */
            Glib::signal_idle().connect_once( [&] {
                if ( !session ) {
                    return;
                }

                logger.Info( "connection", "disconnecting" );
                session->runtime->RequestStop();
                session->runtime->Join();
                logger.Info( "connection", "disconnected" );
                session.reset();
                application->release();
            } );
        } );

        application->add_window( window );
        window.present();
    } );

    /* The endpoint was consumed above, so GTK only sees the program name. */
    const int status = application->run( 1, argv );
    return exitCode != 0 ? exitCode : status;
}
