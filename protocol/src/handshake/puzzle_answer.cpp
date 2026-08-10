#include <protocol/handshake/puzzle_answer.hpp>
#include <utility>

namespace ts::protocol {

    PuzzleAnswer::PuzzleAnswer( const Puzzle& puzzle,
                                const std::array<std::byte, 64>& solution,
                                std::vector<std::byte> clientInitIv,
                                std::uint32_t clientVersion ):
        ClientHandshakeMessage<handshake::PuzzleAnswerCommand>( clientVersion ), m_Puzzle( puzzle ), m_Solution( solution ),
        m_ClientInitIv( std::move( clientInitIv ) ) {
    }

    Packet PuzzleAnswer::Serialize() const {
        BinaryWriter writer = CreateWriter();

        writer.WriteBytes( m_Puzzle.X() );

        writer.WriteBytes( m_Puzzle.N() );

        writer.WriteU32( m_Puzzle.Level() );

        writer.WriteBytes( m_Puzzle.ServerData() );

        writer.WriteBytes( m_Solution );

        writer.WriteBytes( m_ClientInitIv );

        return CreatePacket( std::move( writer ) );
    }

} // namespace ts::protocol
