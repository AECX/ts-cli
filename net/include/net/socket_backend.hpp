#ifndef TS_NET_SOCKET_BACKEND_HPP
#define TS_NET_SOCKET_BACKEND_HPP

#include <chrono>
#include <cstddef>
#include <memory>
#include <net/address.hpp>

namespace ts::net {

    /*
     * Platform socket primitives behind a stable interface. UdpSocket
     * owns exactly one of these; only this implementation differs
     * between build targets (POSIX today, Win32 later), so the rest
     * of the codebase never depends on OS networking headers.
     */
    class SocketBackend {
      public:
        virtual ~SocketBackend() = default;

        virtual void Connect() = 0;
        virtual void Send( const void* data, std::size_t size ) = 0;
        virtual void Wake() = 0;

        [[nodiscard]] virtual std::size_t Receive( void* buffer, std::size_t size ) = 0;
        [[nodiscard]] virtual bool WaitReadable( std::chrono::milliseconds timeout ) const = 0;
    };

    [[nodiscard]] std::unique_ptr<SocketBackend> CreateSocketBackend( const Address& address );

} // namespace ts::net

#endif // TS_NET_SOCKET_BACKEND_HPP
