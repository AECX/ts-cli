#ifndef TS_NET_UDP_SOCKET_HPP
#define TS_NET_UDP_SOCKET_HPP

#include "address.hpp"

#include <chrono>
#include <cstddef>
#include <memory>

namespace ts::net {

    class SocketBackend;

    class UdpSocket {
      public:
        explicit UdpSocket( const Address& address );
        ~UdpSocket();

        UdpSocket( const UdpSocket& ) = delete;
        UdpSocket& operator=( const UdpSocket& ) = delete;

        void Connect();
        void Send( const void* data, std::size_t size );
        void Wake();

        [[nodiscard]] std::size_t Receive( void* buffer, std::size_t size );
        [[nodiscard]] bool WaitReadable( std::chrono::milliseconds timeout ) const;

      private:
        std::unique_ptr<SocketBackend> m_Backend;
    };

} // namespace ts::net

#endif // TS_NET_UDP_SOCKET_HPP
