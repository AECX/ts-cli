#include <client/gtk/platform/shutdown_signal.hpp>
#include <functional>
#include <glib.h>
#include <utility>
#include <windows.h>

namespace ts::client::gtk::platform {

    namespace {

        /*
         * Lives for the rest of the process: the console handler and the
         * session window below keep pointing at it while the main loop runs.
         */
        std::function<void()> Handler;

        gboolean RunHandler( gpointer ) {
            Handler();
            return G_SOURCE_REMOVE;
        }

        /* Runs on a thread the system creates; hand the request to the main loop. */
        BOOL WINAPI HandleConsoleControl( DWORD ) {
            (void)g_idle_add( RunHandler, nullptr );
            return TRUE;
        }

        /*
         * Top-level windows (not message-only ones) receive the session-end
         * broadcast. GDK pumps this thread's message queue, so this runs on
         * the main loop. The process may be terminated as soon as
         * WM_ENDSESSION returns, so the handler runs right away and gets one
         * pass over pending main-loop work; disconnecting cleanly is best
         * effort here, as it is for any Windows program at logoff.
         */
        LRESULT CALLBACK SessionWindowProcedure( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
            if ( message == WM_QUERYENDSESSION ) {
                return TRUE;
            }

            if ( message == WM_ENDSESSION && wParam != FALSE ) {
                Handler();

                while ( g_main_context_iteration( nullptr, FALSE ) ) {
                }

                return 0;
            }

            return DefWindowProcW( window, message, wParam, lParam );
        }

        void CreateSessionWindow() {
            const HINSTANCE instance = GetModuleHandleW( nullptr );

            WNDCLASSW windowClass {};
            windowClass.lpfnWndProc = SessionWindowProcedure;
            windowClass.hInstance = instance;
            windowClass.lpszClassName = L"ts-gtk-session";

            if ( RegisterClassW( &windowClass ) == 0 ) {
                return;
            }

            /* Never shown; WS_POPUP keeps it out of the taskbar. */
            (void)CreateWindowExW( 0,
                                   windowClass.lpszClassName,
                                   L"ts-gtk",
                                   WS_POPUP,
                                   0,
                                   0,
                                   0,
                                   0,
                                   nullptr,
                                   nullptr,
                                   instance,
                                   nullptr );
        }

    } // namespace

    void InstallShutdownHandler( std::function<void()> onShutdown ) {
        Handler = std::move( onShutdown );

        /* Only has an effect when started with a console attached. */
        (void)SetConsoleCtrlHandler( HandleConsoleControl, TRUE );
        CreateSessionWindow();
    }

} // namespace ts::client::gtk::platform
