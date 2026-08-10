#include <algorithm>
#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <client/cli/channel_tree_view.hpp>
#include <client/cli/target_resolver.hpp>
#include <client/config/user_config.hpp>
#include <client/runtime/action.hpp>
#include <client/runtime/action_processor.hpp>
#include <client/runtime/event.hpp>
#include <client/runtime/event_queue.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <protocol/connection.hpp>
#include <protocol/message/text_message.hpp>
#include <protocol/state/channel.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client.hpp>
#include <protocol/state/client_store.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ts::client {

    ActionProcessor::ActionProcessor( EventQueue& eventQueue, UserConfigStore& userConfigStore, audio::AudioEngine& audio ):
        m_EventQueue( eventQueue ), m_UserConfigStore( userConfigStore ), m_Audio( audio ) {
    }

    void ActionProcessor::Process( protocol::Connection& connection, const ClientAction& action ) {
        std::visit(
            [this, &connection]( const auto& value ) {
                using ActionType = std::decay_t<decltype( value )>;

                if constexpr ( std::is_same_v<ActionType, SendCurrentChannelMessageAction> ) {
                    const std::uint64_t channelId = connection.CurrentChannelId();

                    if ( channelId == 0 || connection.Channels().Find( channelId ) == nullptr ) {
                        PushError( "Current channel is unknown" );
                        return;
                    }

                    connection.SendTextMessage(
                        protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Channel, .id = channelId },
                        value.text );
                } else if constexpr ( std::is_same_v<ActionType, SendPrivateMessageAction> ) {
                    const auto candidates = ClientCandidates( connection );
                    const auto target = cli::TargetResolver::ResolvePrivate( candidates, value.target );

                    if ( !target ) {
                        PushError( "No client matches '" + value.target + "'" );
                        return;
                    }

                    connection.SendTextMessage(
                        protocol::TextMessageTarget { .mode = protocol::TextMessageTargetMode::Private, .id = target->id },
                        value.text );
                } else if constexpr ( std::is_same_v<ActionType, SendReplyMessageAction> ) {
                    connection.SendTextMessage( value.target, value.text );
                } else if constexpr ( std::is_same_v<ActionType, JoinChannelAction> ) {
                    const auto candidates = ChannelCandidates( connection );
                    const auto target = cli::TargetResolver::ResolveChannel( candidates, value.channel );

                    if ( !target ) {
                        PushError( "No channel matches '" + value.channel + "'" );
                        return;
                    }

                    connection.MoveToChannel( target->id );
                } else if constexpr ( std::is_same_v<ActionType, ChangeNicknameAction> ) {
                    connection.ChangeNickname( value.nickname );
                } else if constexpr ( std::is_same_v<ActionType, ListTreeAction> ) {
                    ProcessListTree( connection, value );
                } else if constexpr ( std::is_same_v<ActionType, UserSettingsAction> ) {
                    ProcessUserSettings( connection, value );
                }
            },
            action );
    }

    void ActionProcessor::ProcessListTree( protocol::Connection& connection, const ListTreeAction& action ) {
        const std::vector<protocol::ChannelTreeEntry> tree = connection.Channels().Tree();

        cli::ChannelTreeRange range { .begin = 0, .end = tree.size(), .baseDepth = 0, .rootIsUnadorned = false };

        if ( action.start ) {
            const auto candidates = ChannelCandidates( connection );
            std::optional<cli::ResolvedTarget> target;

            for ( const cli::TargetCandidate& candidate : candidates ) {
                if ( std::to_string( candidate.id ) == *action.start ) {
                    target = cli::ResolvedTarget { .kind = candidate.kind,
                                                   .id = candidate.id,
                                                   .name = std::string( candidate.name ) };
                    break;
                }
            }

            if ( !target ) {
                target = cli::TargetResolver::ResolveChannel( candidates, *action.start );
            }

            if ( !target ) {
                PushError( "No channel matches '" + *action.start + "'" );
                return;
            }

            const auto entry = std::find_if( tree.begin(), tree.end(), [target]( const protocol::ChannelTreeEntry& item ) {
                return item.channel != nullptr && item.channel->id == target->id;
            } );

            if ( entry == tree.end() ) {
                PushError( "Channel tree is inconsistent" );
                return;
            }

            range.begin = static_cast<std::size_t>( std::distance( tree.begin(), entry ) );
            range.baseDepth = entry->depth;
            range.rootIsUnadorned = true;
            range.end = range.begin + 1;

            while ( range.end < tree.size() && tree[range.end].depth > range.baseDepth ) {
                ++range.end;
            }
        }

        PushInfo( action.start ? "channel/client tree from " + *action.start + ':' : "channel/client tree:" );

        for ( std::string& line : cli::FormatChannelTree( tree, range, connection.Clients(), connection.ClientId() ) ) {
            PushInfo( std::move( line ) );
        }
    }

    void ActionProcessor::ProcessUserSettings( protocol::Connection& connection, const UserSettingsAction& action ) {
        const auto candidates = ClientCandidates( connection );
        const cli::UniqueTargetResolution resolution = cli::TargetResolver::ResolveUniqueClient( candidates, action.target );

        if ( resolution.status == cli::UniqueTargetStatus::Ambiguous ) {
            PushError( "Client name '" + action.target + "' is ambiguous; use #<client-id>" );
            return;
        }
        if ( resolution.status == cli::UniqueTargetStatus::NoMatch || !resolution.target ) {
            PushError( "No client matches '" + action.target + "'" );
            return;
        }

        const auto clientId = static_cast<std::uint16_t>( resolution.target->id );
        if ( clientId == connection.ClientId() ) {
            PushError( "Per-user playback settings cannot target yourself" );
            return;
        }

        const protocol::Client* client = connection.Clients().Find( clientId );
        if ( client == nullptr || !client->detailsKnown || client->uniqueId.empty() ) {
            PushError( "Client identity is not known yet" );
            return;
        }

        UserConfig config = m_UserConfigStore.Load( client->uniqueId );

        switch ( action.kind ) {
            case cli::UserCommandKind::Inspect:
                break;

            case cli::UserCommandKind::VolumeDb:
                config.volumeDb = action.numberValue;
                break;

            case cli::UserCommandKind::VolumePercent:
                if ( action.numberValue <= 0.0F ) {
                    config.volumeDb = UserConfigStore::MinVolumeDb();
                } else {
                    config.volumeDb = 20.0F * std::log10( action.numberValue / 100.0F );
                    config.volumeDb =
                        std::clamp( config.volumeDb, UserConfigStore::MinVolumeDb(), UserConfigStore::MaxVolumeDb() );
                }
                break;

            case cli::UserCommandKind::VolumeReset:
                config.volumeDb = 0.0F;
                break;

            case cli::UserCommandKind::Mute:
                config.muted = true;
                break;

            case cli::UserCommandKind::Unmute:
                config.muted = false;
                break;
        }

        if ( action.kind != cli::UserCommandKind::Inspect ) {
            m_UserConfigStore.Save( client->uniqueId, config );
            m_Audio.SetTalkerSettings( clientId, audio::TalkerSettings { .volumeDb = config.volumeDb, .muted = config.muted } );
        }

        std::ostringstream line;
        line << client->nickname << " [" << client->id << "]: volume=" << config.volumeDb
             << " dB, muted=" << ( config.muted ? "yes" : "no" ) << ", uid=" << client->uniqueId;
        PushInfo( line.str() );
    }

    std::vector<cli::TargetCandidate> ActionProcessor::ClientCandidates( const protocol::Connection& connection ) {
        std::vector<cli::TargetCandidate> candidates;

        for ( const protocol::Client* client : connection.Clients().All() ) {
            if ( client->detailsKnown && !client->nickname.empty() ) {
                candidates.push_back(
                    cli::TargetCandidate { .kind = cli::TargetKind::Client, .id = client->id, .name = client->nickname } );
            }
        }

        return candidates;
    }

    std::vector<cli::TargetCandidate> ActionProcessor::ChannelCandidates( const protocol::Connection& connection ) {
        std::vector<cli::TargetCandidate> candidates;

        for ( const protocol::ChannelTreeEntry& entry : connection.Channels().Tree() ) {
            candidates.push_back( cli::TargetCandidate { .kind = cli::TargetKind::Channel,
                                                         .id = entry.channel->id,
                                                         .name = entry.channel->name } );
        }

        return candidates;
    }

    void ActionProcessor::PushError( std::string message ) {
        m_EventQueue.Push( ActionErrorEvent { .message = std::move( message ) } );
    }

    void ActionProcessor::PushInfo( std::string message ) {
        m_EventQueue.Push( ActionInfoEvent { .message = std::move( message ) } );
    }

} // namespace ts::client
