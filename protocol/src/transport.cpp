#include <array>
#include <chrono>
#include <cstddef>
#include <protocol/transport.hpp>
#include <vector>

namespace ts::protocol {

    Transport::Transport( net::UdpSocket& socket ): m_Socket( socket ) {
    }

    void Transport::Send( const Packet& packet ) {
        m_Socket.Send( packet.Data().data(), packet.Size() );
    }

    Packet Transport::Receive() {
        std::array<std::byte, 4096> buffer {};

        const std::size_t received = m_Socket.Receive( buffer.data(), buffer.size() );

        return Packet( std::vector<std::byte>( buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>( received ) ) );
    }

    bool Transport::WaitReadable( std::chrono::milliseconds timeout ) const {
        return m_Socket.WaitReadable( timeout );
    }

} // namespace ts::protocol
