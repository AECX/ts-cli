#include <algorithm>
#include <protocol/handshake/set_cookie.hpp>
#include <stdexcept>

namespace ts::protocol {

    SetCookie SetCookie::Parse( const Packet& packet ) {
        BinaryReader reader = CreateReader( packet );

        SetCookie result;

        const auto serverData = reader.ReadBytes( result.m_ServerData.size() );

        std::copy( serverData.begin(), serverData.end(), result.m_ServerData.begin() );

        const auto randomData = reader.ReadBytes( result.m_RandomData.size() );

        std::copy( randomData.begin(), randomData.end(), result.m_RandomData.begin() );

        const std::size_t remaining = reader.Remaining();

        // Standard TS3 SET_COOKIE ends here.
        //
        // TS6 Compatible endpoint appends 3 additional bytes
        // so we tolerate that aswell.
        if ( remaining == 3 ) {
            reader.Skip( 3 );
        } else if ( remaining != 0 ) {
            throw std::runtime_error( "Unexpected SET_COOKIE payload size" );
        }

        return result;
    }

    const std::array<std::byte, 16>& SetCookie::ServerData() const {
        return m_ServerData;
    }

    const std::array<std::byte, 4>& SetCookie::RandomData() const {
        return m_RandomData;
    }

} // namespace ts::protocol
