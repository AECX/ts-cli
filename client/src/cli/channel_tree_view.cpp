#include <client/cli/channel_tree_view.hpp>
#include <cstdint>
#include <protocol/state/channel.hpp>
#include <protocol/state/client.hpp>
#include <protocol/state/client_store.hpp>
#include <sstream>
#include <string_view>

namespace ts::client::cli {

    namespace {

        bool IsLastSibling( const std::vector<protocol::ChannelTreeEntry>& tree, std::size_t index, std::size_t end ) {
            const std::size_t depth = tree[index].depth;

            for ( std::size_t next = index + 1; next < end; ++next ) {
                if ( tree[next].depth <= depth ) {
                    return tree[next].depth != depth;
                }
            }

            return true;
        }

        void AppendPrefix( std::ostringstream& line, const std::vector<bool>& ancestorLast, std::size_t columns ) {
            for ( std::size_t column = 0; column < columns; ++column ) {
                line << ( ancestorLast[column] ? "    " : "│   " );
            }
        }

    } // namespace

    std::vector<std::string> FormatChannelTree( const std::vector<protocol::ChannelTreeEntry>& tree,
                                                const ChannelTreeRange& range,
                                                const protocol::ClientStore& clients,
                                                std::uint16_t selfClientId ) {
        std::vector<std::string> lines;
        std::vector<bool> ancestorLast;

        for ( std::size_t index = range.begin; index < range.end; ++index ) {
            const protocol::ChannelTreeEntry& entry = tree[index];

            if ( entry.channel == nullptr ) {
                continue;
            }

            const std::size_t depth = entry.depth >= range.baseDepth ? entry.depth - range.baseDepth : 0;
            const bool isRoot = range.rootIsUnadorned && depth == 0;
            const bool isLast = IsLastSibling( tree, index, range.end );

            ancestorLast.resize( depth );

            std::ostringstream channelLine;

            if ( !isRoot ) {
                AppendPrefix( channelLine, ancestorLast, depth );
                channelLine << ( isLast ? "└── " : "├── " );
            }

            channelLine << '[' << entry.channel->id << "] " << entry.channel->name;

            if ( entry.channel->defaultChannel ) {
                channelLine << " [default]";
            }
            if ( entry.channel->passwordProtected ) {
                channelLine << " [password]";
            }
            if ( entry.channel->permanent ) {
                channelLine << " [permanent]";
            } else if ( entry.channel->semiPermanent ) {
                channelLine << " [semi-permanent]";
            }

            lines.push_back( channelLine.str() );

            ancestorLast.push_back( isLast );

            const bool hasSubchannel = index + 1 < range.end && tree[index + 1].depth > entry.depth;
            const std::vector<const protocol::Client*> channelClients = clients.InChannel( entry.channel->id );

            for ( std::size_t clientIndex = 0; clientIndex < channelClients.size(); ++clientIndex ) {
                const protocol::Client& client = *channelClients[clientIndex];
                const bool isLastClient = clientIndex + 1 == channelClients.size() && !hasSubchannel;

                std::ostringstream clientLine;
                AppendPrefix( clientLine, ancestorLast, depth + 1 );
                clientLine << ( isLastClient ? "└── " : "├── " );

                if ( client.detailsKnown && !client.nickname.empty() ) {
                    clientLine << client.nickname;
                } else {
                    clientLine << "client " << client.id;
                }

                clientLine << " [" << client.id << ']';
                clientLine << FormatClientStatus( client, selfClientId );

                lines.push_back( clientLine.str() );
            }
        }

        return lines;
    }

    std::string FormatClientStatus( const protocol::Client& client, std::uint16_t selfId ) {
        std::string result;

        const auto add = [&result]( std::string_view status ) {
            result += " [";
            result += status;
            result += ']';
        };

        if ( client.id == selfId ) {
            add( "you" );
        }
        if ( !client.detailsKnown ) {
            add( "loading" );
            return result;
        }
        if ( client.away ) {
            add( "afk" );
        }
        if ( client.inputMuted ) {
            add( "mic muted" );
        }
        if ( client.outputMuted ) {
            add( "sound muted" );
        }
        if ( !client.inputHardware ) {
            add( "mic disabled" );
        }
        if ( !client.outputHardware ) {
            add( "sound disabled" );
        }
        if ( client.recording ) {
            add( "recording" );
        }
        if ( client.prioritySpeaker ) {
            add( "priority speaker" );
        }
        if ( client.channelCommander ) {
            add( "channel commander" );
        }
        if ( client.serverQuery ) {
            add( "query" );
        }

        return result;
    }

} // namespace ts::client::cli
