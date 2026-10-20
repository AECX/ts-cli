#include <client/runtime/server_snapshot.hpp>
#include <protocol/connection.hpp>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <string>

namespace ts::client {

    ServerSnapshot CaptureServerSnapshot( const protocol::Connection& connection ) {
        ServerSnapshot snapshot;
        snapshot.serverName = std::string( connection.ServerName() );
        snapshot.clientId = connection.ClientId();
        snapshot.currentChannelId = connection.CurrentChannelId();

        for ( const protocol::ChannelTreeEntry& entry : connection.Channels().Tree() ) {
            if ( entry.channel != nullptr ) {
                snapshot.channels.push_back( ChannelSnapshot { .channel = *entry.channel, .depth = entry.depth } );
            }
        }

        for ( const protocol::Client* client : connection.Clients().All() ) {
            if ( client != nullptr ) {
                snapshot.clients.push_back( *client );
            }
        }

        return snapshot;
    }

} // namespace ts::client
