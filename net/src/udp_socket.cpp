#include <net/socket_backend.hpp>
#include <net/udp_socket.hpp>

namespace ts::net {

    UdpSocket::UdpSocket( const Address& address ): m_Backend( CreateSocketBackend( address ) ) {
    }

    UdpSocket::~UdpSocket() = default;

    void UdpSocket::Connect() {
        m_Backend->Connect();
    }

    void UdpSocket::Send( const void* data, std::size_t size ) {
        m_Backend->Send( data, size );
    }

    void UdpSocket::Wake() {
        m_Backend->Wake();
    }

    std::size_t UdpSocket::Receive( void* buffer, std::size_t size ) {
        return m_Backend->Receive( buffer, size );
    }

    bool UdpSocket::WaitReadable( std::chrono::milliseconds timeout ) const {
        return m_Backend->WaitReadable( timeout );
    }

} // namespace ts::net
