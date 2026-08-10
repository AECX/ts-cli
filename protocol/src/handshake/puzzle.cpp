#include <algorithm>
#include <protocol/handshake/puzzle.hpp>
#include <stdexcept>

namespace ts::protocol {

    Puzzle Puzzle::Parse( const Packet& packet ) {
        BinaryReader reader = CreateReader( packet );

        Puzzle result;

        const auto x = reader.ReadBytes( result.m_X.size() );

        std::copy( x.begin(), x.end(), result.m_X.begin() );

        const auto n = reader.ReadBytes( result.m_N.size() );

        std::copy( n.begin(), n.end(), result.m_N.begin() );

        result.m_Level = reader.ReadU32();

        const auto serverData = reader.ReadBytes( result.m_ServerData.size() );

        std::copy( serverData.begin(), serverData.end(), result.m_ServerData.begin() );

        if ( reader.Remaining() != 0 ) {
            throw std::runtime_error( "Unexpected trailing puzzle data" );
        }

        return result;
    }

    const std::array<std::byte, 64>& Puzzle::X() const {
        return m_X;
    }

    const std::array<std::byte, 64>& Puzzle::N() const {
        return m_N;
    }

    std::uint32_t Puzzle::Level() const {
        return m_Level;
    }

    const std::array<std::byte, 100>& Puzzle::ServerData() const {
        return m_ServerData;
    }

} // namespace ts::protocol
