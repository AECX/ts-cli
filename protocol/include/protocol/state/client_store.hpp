#ifndef TS_PROTOCOL_STATE_CLIENT_STORE_HPP
#define TS_PROTOCOL_STATE_CLIENT_STORE_HPP

#include "client.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <protocol/message/client_enter_view.hpp>
#include <protocol/message/client_left_view.hpp>
#include <protocol/message/client_moved.hpp>
#include <protocol/message/client_updated.hpp>
#include <vector>

namespace ts::protocol {

    class ChannelStore;

    class ClientStore {
      public:
        void Clear();

        void Apply( const ClientEnterView& message );
        void Apply( const ClientMoved& message );
        void Apply( const ClientLeftView& message );
        void Apply( const ClientUpdated& message );
        void Move( std::uint16_t clientId, std::uint64_t channelId );
        void Remove( std::uint16_t clientId );
        void RemoveInChannel( std::uint64_t channelId, std::uint16_t preserveClientId = 0 );

        void Validate( const ChannelStore& channels ) const;

        [[nodiscard]] const Client* Find( std::uint16_t clientId ) const;
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] std::vector<const Client*> All() const;
        [[nodiscard]] std::vector<const Client*> InChannel( std::uint64_t channelId ) const;

      private:
        std::map<std::uint16_t, Client> m_Clients;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_STATE_CLIENT_STORE_HPP
