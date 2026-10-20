#include <audio/audio_engine.hpp>
#include <client/gtk/main_window.hpp>
#include <client/gtk/platform/shutdown_signal.hpp>
#include <client/gtk/session.hpp>
#include <config/audio_settings.hpp>
#include <config/config.hpp>
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
#include <string>
#include <string_view>

namespace {

    /* Everything that outlives a single connection, created once the application is activated. */
    struct Application {
        explicit Application( ts::log::Logger& logger );

        ts::config::Paths paths;
        ts::config::Config config;
        ts::client::gtk::NetworkWake wake;
        ts::audio::AudioEngine audio;
        ts::config::UserConfigStore userConfigStore;
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

    Application::Application( ts::log::Logger& logger ):
        paths( DiscoverPaths() ), config( LoadConfig( paths, logger ) ), audio( [this] {
            wake.Wake();
        } ),
        userConfigStore( paths.UsersDirectory() ) {
        ts::config::ApplyAudioSettings( audio, config.AudioSettings(), [&logger]( std::string_view message ) {
            logger.Warning( "audio", std::string( message ) );
        } );

        window.emplace( audio, config, paths, userConfigStore, wake );
    }

} // namespace

int main( int argc, char* argv[] ) {
    if ( argc > 2 ) {
        std::cerr << "Usage: ts-gtk [endpoint]" << std::endl;
        return 1;
    }

    ts::log::OstreamSink sink( std::cerr, false );
    ts::log::Logger logger( sink, ts::log::Level::Info );

    /*
     * glibmm would otherwise switch the global C++ locale to the user's,
     * which config.conf's number formatting and parsing must not follow.
     */
    Glib::set_init_to_users_preferred_locale( false );

    const std::optional<std::string> endpoint = argc == 2 ? std::optional<std::string>( argv[1] ) : std::nullopt;
    const Glib::RefPtr<Gtk::Application> application = Gtk::Application::create( "", Gio::Application::Flags::NON_UNIQUE );
    std::unique_ptr<Application> app;
    int exitCode = 0;

    /*
     * gtkmm 4 widgets can only be created once the application is
     * activated, and must be gone before run() returns. The application is
     * held until the window has closed and disconnected, or the error
     * dialog closed.
     */
    application->signal_activate().connect( [&] {
        application->hold();

        try {
            app = std::make_unique<Application>( logger );
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

        ts::client::gtk::MainWindow& window = *app->window;

        ts::client::gtk::platform::InstallShutdownHandler( [&app] {
            if ( app ) {
                app->window->close();
            }
        } );

        window.signal_hide().connect( [&] {
            /* Deferred: the window must not be destroyed from inside its own hide handler. */
            Glib::signal_idle().connect_once( [&] {
                if ( !app ) {
                    return;
                }

                app->window->Shutdown();
                app.reset();
                application->release();
            } );
        } );

        application->add_window( window );
        window.present();

        if ( endpoint ) {
            window.Connect( *endpoint );
        }
    } );

    /* The endpoint was consumed above, so GTK only sees the program name. */
    const int status = application->run( 1, argv );
    return exitCode != 0 ? exitCode : status;
}
