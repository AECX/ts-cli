#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <protocol/state/channel_store.hpp>
#include <protocol/state/client_store.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    void ClientStore::Clear() {
        m_Clients.clear();
    }

    void ClientStore::Apply( const ClientEnterView& message ) {
        for ( const ClientEnterViewEntry& entry : message.Entries() ) {
            std::uint64_t channelId = entry.channelId;

            const auto existing = m_Clients.find( entry.id );

            /*
             * If a move notification arrived before the full
             * enter-view notification, preserve that newer
             * channel placement when ctid is omitted here.
             */
            if ( existing != m_Clients.end() && existing->second.channelId != 0 &&
                 ( channelId == 0 || !existing->second.detailsKnown ) ) {
                channelId = existing->second.channelId;
            }

            std::string uniqueId = entry.uniqueId;
            if ( uniqueId.empty() && existing != m_Clients.end() ) {
                uniqueId = existing->second.uniqueId;
            }

            Client client { .id = entry.id,
                            .channelId = channelId,
                            .nickname = entry.nickname,
                            .uniqueId = std::move( uniqueId ),
                            .away = entry.away,
                            .inputMuted = entry.inputMuted,
                            .outputMuted = entry.outputMuted,
                            .inputHardware = entry.inputHardware,
                            .outputHardware = entry.outputHardware,
                            .recording = entry.recording,
                            .prioritySpeaker = entry.prioritySpeaker,
                            .channelCommander = entry.channelCommander,
                            .serverQuery = entry.serverQuery,
                            .detailsKnown = true };

            m_Clients.insert_or_assign( client.id, std::move( client ) );
        }
    }

    void ClientStore::Apply( const ClientMoved& message ) {
        for ( const ClientMovedEntry& entry : message.Entries() ) {
            Move( entry.id, entry.channelId );
        }
    }

    void ClientStore::Apply( const ClientLeftView& message ) {
        for ( const ClientLeftViewEntry& entry : message.Entries() ) {
            m_Clients.erase( entry.id );
        }
    }

    void ClientStore::Apply( const ClientUpdated& message ) {
        for ( const ClientUpdatedEntry& entry : message.Entries() ) {
            auto [client, inserted] = m_Clients.try_emplace( entry.id );

            (void)inserted;

            client->second.id = entry.id;

            if ( entry.nickname ) {
                client->second.nickname = *entry.nickname;
            }

            if ( entry.away ) {
                client->second.away = *entry.away;
            }

            if ( entry.inputMuted ) {
                client->second.inputMuted = *entry.inputMuted;
            }

            if ( entry.outputMuted ) {
                client->second.outputMuted = *entry.outputMuted;
            }

            if ( entry.inputHardware ) {
                client->second.inputHardware = *entry.inputHardware;
            }

            if ( entry.outputHardware ) {
                client->second.outputHardware = *entry.outputHardware;
            }

            if ( entry.recording ) {
                client->second.recording = *entry.recording;
            }

            if ( entry.prioritySpeaker ) {
                client->second.prioritySpeaker = *entry.prioritySpeaker;
            }

            if ( entry.channelCommander ) {
                client->second.channelCommander = *entry.channelCommander;
            }
        }
    }

    void ClientStore::Move( std::uint16_t clientId, std::uint64_t channelId ) {
        auto [client, inserted] = m_Clients.try_emplace( clientId );
        (void)inserted;
        client->second.id = clientId;
        client->second.channelId = channelId;
    }

    void ClientStore::Remove( std::uint16_t clientId ) {
        m_Clients.erase( clientId );
    }

    void ClientStore::RemoveInChannel( std::uint64_t channelId, std::uint16_t preserveClientId ) {
        for ( auto client = m_Clients.begin(); client != m_Clients.end(); ) {
            if ( client->second.channelId == channelId && client->second.id != preserveClientId ) {
                client = m_Clients.erase( client );
            } else {
                ++client;
            }
        }
    }

    void ClientStore::Validate( const ChannelStore& channels ) const {
        for ( const auto& [id, client] : m_Clients ) {
            (void)id;

            if ( client.channelId == 0 ) {
                continue;
            }

            if ( channels.Find( client.channelId ) == nullptr ) {
                throw std::runtime_error( "Client references an unknown channel" );
            }
        }
    }

    const Client* ClientStore::Find( std::uint16_t clientId ) const {
        const auto client = m_Clients.find( clientId );

        if ( client == m_Clients.end() ) {
            return nullptr;
        }

        return &client->second;
    }

    std::size_t ClientStore::Size() const {
        return m_Clients.size();
    }

    std::vector<const Client*> ClientStore::All() const {
        std::vector<const Client*> result;
        result.reserve( m_Clients.size() );

        for ( const auto& [id, client] : m_Clients ) {
            (void)id;
            result.push_back( &client );
        }

        return result;
    }

    std::vector<const Client*> ClientStore::InChannel( std::uint64_t channelId ) const {
        std::vector<const Client*> result;

        for ( const auto& [id, client] : m_Clients ) {
            (void)id;

            if ( client.channelId == channelId ) {
                result.push_back( &client );
            }
        }

        std::sort( result.begin(), result.end(), []( const Client* left, const Client* right ) {
            if ( left->detailsKnown != right->detailsKnown ) {
                return left->detailsKnown && !right->detailsKnown;
            }

            if ( left->nickname != right->nickname ) {
                return left->nickname < right->nickname;
            }

            return left->id < right->id;
        } );

        return result;
    }

} // namespace ts::protocol
