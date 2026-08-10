#include <protocol/handshake/get_puzzle.hpp>
#include <utility>

namespace ts::protocol {

    GetPuzzle::GetPuzzle( const SetCookie& cookie, std::uint32_t clientVersion ):
        ClientHandshakeMessage<handshake::GetPuzzleCommand>( clientVersion ), m_Cookie( cookie ) {
    }

    Packet GetPuzzle::Serialize() const {
        BinaryWriter writer = CreateWriter();

        writer.WriteBytes( m_Cookie.ServerData() );

        writer.WriteBytes( m_Cookie.RandomData() );

        return CreatePacket( std::move( writer ) );
    }

} // namespace ts::protocol
