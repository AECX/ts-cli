#include <cstddef>
#include <cstdint>
#include <protocol/command/writer.hpp>
#include <protocol/message/client_move.hpp>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    ClientMove::ClientMove( std::uint16_t clientId, std::uint64_t channelId ):
        m_ClientId( clientId ), m_ChannelId( channelId ) {
        if ( m_ClientId == 0 ) {
            throw std::runtime_error( "Client move client ID is zero" );
        }

        if ( m_ChannelId == 0 ) {
            throw std::runtime_error( "Client move channel ID is zero" );
        }
    }

    std::vector<std::byte> ClientMove::Serialize() const {
        CommandWriter writer( "clientmove" );

        writer.Write( "clid", m_ClientId );
        writer.Write( "cid", m_ChannelId );

        return writer.Take();
    }

} // namespace ts::protocol
