#include "win32_error.hpp"
#include "win32_winsock.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <net/socket_backend.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace ts::net {

    namespace {

        const sockaddr* AsSockaddr( const Address& address ) {
            return reinterpret_cast<const sockaddr*>( address.storage.data() );
        }

        class Win32SocketBackend final: public SocketBackend {
          public:
            explicit Win32SocketBackend( const Address& address ): m_Address( address ) {
                EnsureWinsockInitialized();

                const auto family = reinterpret_cast<const SOCKADDR_STORAGE*>( m_Address.storage.data() )->ss_family;

                m_Socket = ::socket( family, SOCK_DGRAM, IPPROTO_UDP );

                if ( m_Socket == INVALID_SOCKET ) {
                    throw std::runtime_error( "Failed to create UDP socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                const DWORD timeoutMs = 5000;

                if ( ::setsockopt( m_Socket,
                                   SOL_SOCKET,
                                   SO_RCVTIMEO,
                                   reinterpret_cast<const char*>( &timeoutMs ),
                                   sizeof( timeoutMs ) ) == SOCKET_ERROR ) {
                    FailAndClose( "Failed to set receive timeout: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                m_WakeupSocket = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

                if ( m_WakeupSocket == INVALID_SOCKET ) {
                    FailAndClose( "Failed to create UDP wakeup socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                sockaddr_in loopback {};
                loopback.sin_family = AF_INET;
                loopback.sin_addr.s_addr = ::htonl( INADDR_LOOPBACK );
                loopback.sin_port = 0;

                if ( ::bind( m_WakeupSocket, reinterpret_cast<sockaddr*>( &loopback ), sizeof( loopback ) ) == SOCKET_ERROR ) {
                    FailAndClose( "Failed to bind UDP wakeup socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                int loopbackLength = sizeof( loopback );

                if ( ::getsockname( m_WakeupSocket, reinterpret_cast<sockaddr*>( &loopback ), &loopbackLength ) ==
                     SOCKET_ERROR ) {
                    FailAndClose( "Failed to inspect UDP wakeup socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                if ( ::connect( m_WakeupSocket, reinterpret_cast<sockaddr*>( &loopback ), sizeof( loopback ) ) ==
                     SOCKET_ERROR ) {
                    FailAndClose( "Failed to connect UDP wakeup socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                u_long nonBlocking = 1;

                if ( ::ioctlsocket( m_WakeupSocket, static_cast<long>( FIONBIO ), &nonBlocking ) == SOCKET_ERROR ) {
                    FailAndClose( "Failed to configure UDP wakeup socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }
            }

            ~Win32SocketBackend() override {
                if ( m_WakeupSocket != INVALID_SOCKET ) {
                    ::closesocket( m_WakeupSocket );
                }

                if ( m_Socket != INVALID_SOCKET ) {
                    ::closesocket( m_Socket );
                }
            }

            Win32SocketBackend( const Win32SocketBackend& ) = delete;
            Win32SocketBackend& operator=( const Win32SocketBackend& ) = delete;

            void Connect() override {
                if ( ::connect( m_Socket, AsSockaddr( m_Address ), static_cast<int>( m_Address.length ) ) == SOCKET_ERROR ) {
                    throw std::runtime_error( "Failed to connect UDP socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }
            }

            void Send( const void* data, std::size_t size ) override {
                const int result = ::send( m_Socket, static_cast<const char*>( data ), static_cast<int>( size ), 0 );

                if ( result == SOCKET_ERROR ) {
                    throw std::runtime_error( "Failed to send UDP packet: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                if ( static_cast<std::size_t>( result ) != size ) {
                    throw std::runtime_error( "UDP packet was only partially sent" );
                }
            }

            void Wake() override {
                const char value = 1;
                const int result = ::send( m_WakeupSocket, &value, 1, 0 );

                if ( result == SOCKET_ERROR ) {
                    const int error = ::WSAGetLastError();

                    if ( error == WSAEWOULDBLOCK ) {
                        /* A pending wakeup already exists; the wakeup socket is non-blocking and single-slot. */
                        return;
                    }

                    throw std::runtime_error( "Failed to wake UDP poll: " + FormatWsaError( error ) );
                }
            }

            std::size_t Receive( void* buffer, std::size_t size ) override {
                const int result = ::recv( m_Socket, static_cast<char*>( buffer ), static_cast<int>( size ), 0 );

                if ( result == SOCKET_ERROR ) {
                    const int error = ::WSAGetLastError();

                    if ( error == WSAETIMEDOUT ) {
                        throw std::runtime_error( "Receive timed out" );
                    }

                    throw std::runtime_error( "Failed to receive UDP packet: " + FormatWsaError( error ) );
                }

                return static_cast<std::size_t>( result );
            }

            bool WaitReadable( std::chrono::milliseconds timeout ) const override {
                WSAPOLLFD descriptors[2] {
                    { .fd = m_Socket, .events = POLLRDNORM, .revents = 0 },
                    { .fd = m_WakeupSocket, .events = POLLRDNORM, .revents = 0 },
                };

                const int result = ::WSAPoll( descriptors, 2, static_cast<int>( timeout.count() ) );

                if ( result == SOCKET_ERROR ) {
                    throw std::runtime_error( "Failed to poll UDP socket: " + FormatWsaError( ::WSAGetLastError() ) );
                }

                if ( result == 0 ) {
                    return false;
                }

                for ( const WSAPOLLFD& descriptor : descriptors ) {
                    if ( ( descriptor.revents & ( POLLERR | POLLHUP | POLLNVAL ) ) != 0 ) {
                        throw std::runtime_error( "UDP poll descriptor reported an error" );
                    }
                }

                if ( ( descriptors[1].revents & POLLRDNORM ) != 0 ) {
                    char discard[64];

                    while ( ::recv( m_WakeupSocket, discard, sizeof( discard ), 0 ) > 0 ) {
                        /* drain any pending wakeups */
                    }
                }

                return ( descriptors[0].revents & POLLRDNORM ) != 0;
            }

          private:
            [[noreturn]] void FailAndClose( std::string message ) {
                if ( m_WakeupSocket != INVALID_SOCKET ) {
                    ::closesocket( m_WakeupSocket );
                    m_WakeupSocket = INVALID_SOCKET;
                }

                if ( m_Socket != INVALID_SOCKET ) {
                    ::closesocket( m_Socket );
                    m_Socket = INVALID_SOCKET;
                }

                throw std::runtime_error( std::move( message ) );
            }

            Address m_Address;
            SOCKET m_Socket = INVALID_SOCKET;
            SOCKET m_WakeupSocket = INVALID_SOCKET;
        };

    } // namespace

    std::unique_ptr<SocketBackend> CreateSocketBackend( const Address& address ) {
        return std::make_unique<Win32SocketBackend>( address );
    }

} // namespace ts::net
