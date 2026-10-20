#ifndef TS_CLIENT_GTK_PLATFORM_SHUTDOWN_SIGNAL_HPP
#define TS_CLIENT_GTK_PLATFORM_SHUTDOWN_SIGNAL_HPP

#include <functional>

namespace ts::client::gtk::platform {

    /*
     * Runs onShutdown on the GTK main loop when the OS asks the process to
     * stop (SIGINT/SIGTERM on POSIX; console control events and logoff or
     * shutdown on Windows), so the window closes and the session disconnects
     * cleanly instead of being killed mid-session. Requires the default GLib
     * main context, i.e. call it after GTK is initialized.
     */
    void InstallShutdownHandler( std::function<void()> onShutdown );

} // namespace ts::client::gtk::platform

#endif // TS_CLIENT_GTK_PLATFORM_SHUTDOWN_SIGNAL_HPP
