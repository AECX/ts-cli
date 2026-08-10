#include <cstddef>
#include <protocol/command/writer.hpp>
#include <protocol/message/client_update.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ts::protocol {

    ClientUpdate::ClientUpdate( std::string nickname ): m_Nickname( std::move( nickname ) ) {
        if ( m_Nickname.empty() ) {
            throw std::runtime_error( "Client nickname is empty" );
        }
    }

    std::vector<std::byte> ClientUpdate::Serialize() const {
        CommandWriter writer( "clientupdate" );

        writer.Write( "client_nickname", m_Nickname );

        return writer.Take();
    }

} // namespace ts::protocol
