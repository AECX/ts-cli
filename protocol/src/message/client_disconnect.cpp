#include <cstddef>
#include <cstdint>
#include <protocol/command/writer.hpp>
#include <protocol/message/client_disconnect.hpp>
#include <string>
#include <utility>
#include <vector>

namespace ts::protocol {

    ClientDisconnect::ClientDisconnect( std::string reason ): m_Reason( std::move( reason ) ) {
    }

    std::vector<std::byte> ClientDisconnect::Serialize() const {
        CommandWriter writer( "clientdisconnect" );

        writer.Write( "reasonid", std::uint32_t { 8 } );
        writer.Write( "reasonmsg", m_Reason );

        return writer.Take();
    }

} // namespace ts::protocol
