#include <client/gtk/platform/shutdown_signal.hpp>
#include <csignal>
#include <functional>
#include <glib-unix.h>
#include <utility>

namespace ts::client::gtk::platform {

    namespace {

        gboolean HandleSignal( gpointer data ) {
            const auto* onShutdown = static_cast<const std::function<void()>*>( data );
            ( *onShutdown )();
            return G_SOURCE_CONTINUE;
        }

    } // namespace

    void InstallShutdownHandler( std::function<void()> onShutdown ) {
        /*
         * Lives for the rest of the process: the GLib sources below keep
         * pointing at it for as long as the main loop runs.
         */
        static std::function<void()> Handler;
        Handler = std::move( onShutdown );

        (void)g_unix_signal_add( SIGINT, HandleSignal, &Handler );
        (void)g_unix_signal_add( SIGTERM, HandleSignal, &Handler );
    }

} // namespace ts::client::gtk::platform
