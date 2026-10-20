#ifndef TS_CLIENT_GTK_SESSION_HPP
#define TS_CLIENT_GTK_SESSION_HPP

#include <atomic>
#include <audio/audio_engine.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event_queue.hpp>
#include <client/runtime/network_runtime.hpp>
#include <config/user_config.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <protocol/connection.hpp>
#include <string>
#include <thread>

namespace ts::client::gtk {

    /*
     * The audio engine outlives every session but has to wake whichever
     * connection is current. Set() runs on the GTK thread, Wake() on the
     * audio worker.
     */
    class NetworkWake {
      public:
        void Set( protocol::Connection* connection );
        void Wake();

      private:
        std::mutex m_Mutex;
        protocol::Connection* m_Connection = nullptr;
    };

    /*
     * One connection to a server, from connecting to its end. Connecting
     * blocks for up to several seconds, so it runs on a background thread;
     * the GTK thread polls for the outcome and then starts the network
     * runtime, exactly as the terminal client does after connecting.
     */
    class Session {
      public:
        enum class State { Connecting, Connected, Failed };

        Session( protocol::ConnectionOptions options,
                 audio::AudioEngine& audio,
                 config::UserConfigStore& userConfigStore,
                 NetworkWake& wake );

        /* Blocks until a pending connect attempt has finished. */
        ~Session();

        Session( const Session& ) = delete;
        Session& operator=( const Session& ) = delete;

        /* GTK thread: starts the runtime once the background connect succeeded. */
        State Poll();

        /* Why connecting failed (State::Failed). */
        [[nodiscard]] const std::string& Error() const;

        /* Disconnects, or abandons a connect attempt that is still running. */
        void RequestStop();

        /* True once a connected session's runtime has stopped; its last events are still queued. */
        [[nodiscard]] bool Ended() const;

        /* After Ended(): waits for the runtime thread and rethrows what ended it abnormally. */
        void Join();

        [[nodiscard]] ActionQueue& Actions();
        [[nodiscard]] EventQueue& Events();

      private:
        audio::AudioEngine& m_Audio;
        config::UserConfigStore& m_UserConfigStore;
        NetworkWake& m_Wake;

        /* Written by the connect thread; the GTK thread only uses it once m_ConnectFinished is set. */
        std::unique_ptr<protocol::Connection> m_Connection;
        ActionQueue m_Actions;
        EventQueue m_Events;
        std::optional<NetworkRuntime> m_Runtime;

        State m_State = State::Connecting;
        std::atomic<bool> m_ConnectFinished = false;
        std::atomic<bool> m_Abandoned = false;
        std::string m_Error;
        std::thread m_Connector;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_SESSION_HPP
