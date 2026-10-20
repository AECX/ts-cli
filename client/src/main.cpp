#include <audio/audio_engine.hpp>
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
#include <string>
#include <string_view>
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

    if ( argc != 2 ) {
        std::cerr << "Usage: ts-cli <endpoint>" << std::endl;
        return 1;
    }

    ts::log::OstreamSink sink( std::cerr, ts::client::platform::StandardErrorIsTerminal() );
    ts::log::Logger logger( sink, ts::log::Level::Trace );

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
