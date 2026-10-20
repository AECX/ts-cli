#include <atomic>
#include <audio/audio_engine.hpp>
#include <charconv>
#include <chrono>
#include <client/cli/channel_tree_view.hpp>
#include <client/cli/command_handler.hpp>
#include <client/cli/presentation.hpp>
#include <client/platform/console_io.hpp>
#include <client/runtime/action_queue.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <client/runtime/network_runtime.hpp>
#include <config/audio_settings.hpp>
#include <config/config.hpp>
#include <config/identity_store.hpp>
#include <config/paths.hpp>
#include <config/setup.hpp>
#include <config/user_config.hpp>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <log/logger.hpp>
#include <log/ostream_sink.hpp>
#include <protocol/protocol.hpp>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

    volatile std::sig_atomic_t StopRequested = 0;

    void HandleSignal( int ) {
        StopRequested = 1;
    }

    void InstallSignalHandlers() {
        if ( std::signal( SIGINT, HandleSignal ) == SIG_ERR ) {
            throw std::runtime_error( "Failed to install SIGINT handler" );
        }

        if ( std::signal( SIGTERM, HandleSignal ) == SIG_ERR ) {
            throw std::runtime_error( "Failed to install SIGTERM handler" );
        }
    }

    [[nodiscard]] std::uint8_t ParseSecurityLevel( std::string_view value ) {
        std::uint32_t level = 0;
        const auto [end, error] = std::from_chars( value.data(), value.data() + value.size(), level );

        if ( error != std::errc {} || end != value.data() + value.size() ||
             level > ts::config::IdentityStore::MaxSecurityLevel() ) {
            throw std::runtime_error( "Security level must be a number from 0 to " +
                                      std::to_string( ts::config::IdentityStore::MaxSecurityLevel() ) );
        }

        return static_cast<std::uint8_t>( level );
    }

    /*
     * ts-cli --security-level <level>: raises the local identity's security
     * level and exits. The search runs on a worker thread so Ctrl+C can stop
     * it; the identity file is only rewritten once it finished.
     */
    int IncreaseSecurityLevel( std::string_view levelArgument, ts::log::Logger& logger ) {
        const std::uint8_t targetLevel = ParseSecurityLevel( levelArgument );
        const ts::config::Paths paths = ts::config::Paths::Discover();
        const std::filesystem::path identityPath = paths.IdentityFile();

        if ( !std::filesystem::exists( identityPath ) ) {
            throw std::runtime_error( "No identity yet at " + identityPath.string() + "; connect once to create one" );
        }

        ts::config::LocalIdentity identity = ts::config::IdentityStore::Load( identityPath );
        const std::uint8_t currentLevel = identity.securityLevel;

        if ( targetLevel <= currentLevel ) {
            throw std::runtime_error( "The identity already has security level " + std::to_string( currentLevel ) );
        }

        logger.Info( "identity",
                     "raising security level " + std::to_string( currentLevel ) + " to " + std::to_string( targetLevel ) +
                         "; this can take a while, Ctrl+C stops" );

        InstallSignalHandlers();

        std::atomic<std::uint64_t> progress = identity.keyOffset;
        std::atomic<bool> done = false;
        std::exception_ptr failure;
        bool finished = false;

        std::jthread search( [&]( const std::stop_token& stop ) {
            try {
                finished = ts::config::IdentityStore::IncreaseSecurityLevel( identity,
                                                                             targetLevel,
                                                                             stop,
                                                                             [&progress]( std::uint64_t offset ) {
                                                                                 progress = offset;
                                                                             } );
            } catch ( ... ) {
                failure = std::current_exception();
            }

            done = true;
        } );

        auto reported = std::chrono::steady_clock::now();

        while ( !done ) {
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

            if ( StopRequested != 0 ) {
                search.request_stop();
            }

            if ( const auto now = std::chrono::steady_clock::now(); now - reported >= std::chrono::seconds( 10 ) ) {
                reported = now;
                logger.Info( "identity", "still searching, at key offset " + std::to_string( progress.load() ) );
            }
        }

        search.join();

        if ( failure ) {
            std::rethrow_exception( failure );
        }

        if ( !finished ) {
            logger.Info( "identity", "stopped; the identity is unchanged" );
            return 1;
        }

        ts::config::IdentityStore::Save( identityPath, identity );
        logger.Info( "identity",
                     "security level is now " + std::to_string( identity.securityLevel ) + " (key offset " +
                         std::to_string( identity.keyOffset ) + ")" );
        return 0;
    }

    std::string CurrentChannelName( const ts::protocol::Connection& connection ) {
        const std::uint64_t channelId = connection.CurrentChannelId();

        if ( channelId == 0 ) {
            return {};
        }

        const ts::protocol::Channel* channel = connection.Channels().Find( channelId );
        return channel == nullptr ? std::string {} : channel->name;
    }

} // namespace

int main( int argc, char* argv[] ) {
    ts::client::platform::InitializeConsole();

    const bool securityLevelMode = argc == 3 && std::string_view( argv[1] ) == "--security-level";

    if ( argc != 2 && !securityLevelMode ) {
        std::cerr << "Usage: ts-cli <endpoint>" << std::endl;
        std::cerr << "       ts-cli --security-level <level>" << std::endl;
        return 1;
    }

    ts::log::OstreamSink sink( std::cerr, ts::client::platform::StandardErrorIsTerminal() );
    ts::log::Logger logger( sink, ts::log::Level::Trace );

    if ( securityLevelMode ) {
        try {
            return IncreaseSecurityLevel( argv[2], logger );
        } catch ( const std::exception& exception ) {
            logger.Error( "identity", exception.what() );
            return 1;
        }
    }

    logger.Info( "client", "starting ts-cli" );

    try {
        const ts::config::Paths paths = ts::config::Paths::Discover();
        paths.EnsureDirectories();

        const std::filesystem::path configPath = paths.ConfigFile();
        const std::filesystem::path identityPath = paths.IdentityFile();

        ts::config::Config config = std::filesystem::exists( configPath )
                                        ? ts::config::Config::Load( configPath )
                                        : ts::config::ConfigSetup::Run( configPath, identityPath );

        ts::config::LocalIdentity localIdentity = ts::config::IdentityStore::Load( identityPath );

        ts::protocol::ClientProfile profile = config.Profile();
        ts::protocol::Identity identity = std::move( localIdentity.identity );

        logger.Info( "connection", "connecting" );

        ts::protocol::Connection connection( ts::protocol::ConnectionOptions { .endpoint = argv[1],
                                                                               .profile = std::move( profile ),
                                                                               .identity = std::move( identity ),
                                                                               .identityKeyOffset = localIdentity.keyOffset } );

        connection.Connect();

        std::string connectedMessage = "connected";

        if ( !connection.ServerName().empty() ) {
            connectedMessage += " to ";
            connectedMessage += connection.ServerName();
        }

        connectedMessage += " as client ";
        connectedMessage += std::to_string( connection.ClientId() );

        logger.Info( "connection", connectedMessage );

        InstallSignalHandlers();

        ts::client::ActionQueue actionQueue;
        ts::client::EventQueue eventQueue;
        const bool stdoutIsTerminal = ts::client::platform::StandardOutputIsTerminal();

        if ( !stdoutIsTerminal || !ts::client::platform::StandardInputIsTerminal() ) {
            throw std::runtime_error( "ts-cli requires an interactive terminal" );
        }

        ts::audio::AudioEngine audio( [&connection] {
            connection.Wake();
        } );

        const auto notifyCallback = [&audio]( ts::audio::NotificationType type ) {
            audio.Notify( type );
        };

        ts::client::cli::Presentation presentation( std::cout, stdoutIsTerminal, notifyCallback );
        presentation.SetCurrentChannel( CurrentChannelName( connection ) );
        presentation.SetCurrentNickname( connection.CurrentNickname() );

        ts::config::ApplyAudioSettings( audio, config.AudioSettings(), [&logger]( std::string_view message ) {
            logger.Warning( "audio", std::string( message ) );
        } );

        ts::client::cli::CommandHandler commandHandler( actionQueue,
                                                        presentation,
                                                        notifyCallback,
                                                        &audio,
                                                        [&config]( const ts::config::ConfigMutator& mutator ) {
                                                            config.Update( mutator );
                                                        } );

        ts::config::UserConfigStore userConfigStore( paths.UsersDirectory() );
        ts::client::NetworkRuntime runtime( connection, actionQueue, eventQueue, audio, userConfigStore );

        logger.Info( "connection", "session loop running" );
        presentation.PrintInfo( "Type /help for commands" );

        runtime.Start();

        presentation.PrintPrompt();
        bool promptVisible = true;

        using namespace std::chrono_literals;

        bool running = true;

        while ( running && StopRequested == 0 ) {
            ts::client::RuntimeEvent event;
            bool hadEvents = false;

            while ( eventQueue.WaitPop( event, 0ms ) ) {
                if ( !hadEvents && promptVisible ) {
                    presentation.BreakPromptLine();
                    promptVisible = false;
                }

                commandHandler.Observe( event );
                presentation.Print( event );
                hadEvents = true;
            }

            const bool eventQueueClosed = eventQueue.Closed();

            if ( hadEvents && !eventQueueClosed ) {
                presentation.PrintPrompt();
                promptVisible = true;
            }

            if ( eventQueueClosed ) {
                break;
            }

            const ts::client::platform::StandardInputPoll input =
                ts::client::platform::PollStandardInputLine( std::chrono::milliseconds( 50 ) );

            if ( input.status == ts::client::platform::StandardInputStatus::Timeout ) {
                continue;
            }

            if ( input.status == ts::client::platform::StandardInputStatus::Closed ) {
                if ( promptVisible ) {
                    presentation.BreakPromptLine();
                    promptVisible = false;
                }

                running = false;
                break;
            }

            presentation.ClearSubmittedInputLine();

            promptVisible = false;
            running = commandHandler.HandleLine( input.line );

            if ( running ) {
                presentation.PrintPrompt();
                promptVisible = true;
            }
        }

        if ( promptVisible ) {
            presentation.BreakPromptLine();
        }

        if ( StopRequested != 0 || !running ) {
            logger.Info( "connection", "disconnecting" );
        }

        runtime.RequestStop();
        runtime.Join();

        if ( StopRequested != 0 || !running ) {
            logger.Info( "connection", "disconnected" );
        }
    } catch ( const std::exception& exception ) {
        logger.Error( "client", exception.what() );
        return 1;
    }

    return 0;
}
