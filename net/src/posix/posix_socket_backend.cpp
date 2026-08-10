#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <net/socket_backend.hpp>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace ts::net {

    namespace {

        const sockaddr* AsSockaddr( const Address& address ) {
            return reinterpret_cast<const sockaddr*>( address.storage.data() );
        }

        class PosixSocketBackend final: public SocketBackend {
          public:
            explicit PosixSocketBackend( const Address& address ): m_Address( address ) {
                const auto family = reinterpret_cast<const sockaddr_storage*>( m_Address.storage.data() )->ss_family;

                m_FileDescriptor = ::socket( family, SOCK_DGRAM, IPPROTO_UDP );

                if ( m_FileDescriptor == -1 ) {
                    throw std::runtime_error( "Failed to create UDP socket" );
                }

                m_WakeupDescriptor = ::eventfd( 0, EFD_NONBLOCK | EFD_CLOEXEC );

                if ( m_WakeupDescriptor == -1 ) {
                    const int error = errno;
                    ::close( m_FileDescriptor );
                    m_FileDescriptor = -1;
                    throw std::runtime_error( std::string( "Failed to create UDP wakeup descriptor: " ) +
                                              std::strerror( error ) );
                }

                const timeval timeout { .tv_sec = 5, .tv_usec = 0 };

                if ( ::setsockopt( m_FileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof( timeout ) ) == -1 ) {
                    const int error = errno;

                    ::close( m_WakeupDescriptor );
                    m_WakeupDescriptor = -1;
                    ::close( m_FileDescriptor );
                    m_FileDescriptor = -1;

                    throw std::runtime_error( std::string( "Failed to set receive timeout: " ) + std::strerror( error ) );
                }
            }

            ~PosixSocketBackend() override {
                if ( m_WakeupDescriptor != -1 ) {
                    ::close( m_WakeupDescriptor );
                }

                if ( m_FileDescriptor != -1 ) {
                    ::close( m_FileDescriptor );
                }
            }

            PosixSocketBackend( const PosixSocketBackend& ) = delete;
            PosixSocketBackend& operator=( const PosixSocketBackend& ) = delete;

            void Connect() override {
                const int status =
                    ::connect( m_FileDescriptor, AsSockaddr( m_Address ), static_cast<socklen_t>( m_Address.length ) );

                if ( status == -1 ) {
                    throw std::runtime_error( std::string( "Failed to connect UDP socket: " ) + std::strerror( errno ) );
                }
            }

            void Send( const void* data, std::size_t size ) override {
                const ssize_t result = ::send( m_FileDescriptor, data, size, 0 );

                if ( result == -1 ) {
                    throw std::runtime_error( std::string( "Failed to send UDP packet: " ) + std::strerror( errno ) );
                }

                if ( static_cast<std::size_t>( result ) != size ) {
                    throw std::runtime_error( "UDP packet was only partially sent" );
                }
            }

            void Wake() override {
                const std::uint64_t value = 1;

                while ( true ) {
                    const ssize_t result = ::write( m_WakeupDescriptor, &value, sizeof( value ) );

                    if ( result == static_cast<ssize_t>( sizeof( value ) ) ) {
                        return;
                    }

                    if ( result == -1 && errno == EINTR ) {
                        continue;
                    }

                    if ( result == -1 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
                        /* A pending wakeup already exists; eventfd is intentionally coalescing. */
                        return;
                    }

                    if ( result == -1 ) {
                        throw std::runtime_error( std::string( "Failed to wake UDP poll: " ) + std::strerror( errno ) );
                    }

                    throw std::runtime_error( "Failed to write complete UDP wakeup value" );
                }
            }

            std::size_t Receive( void* buffer, std::size_t size ) override {
                const ssize_t result = ::recv( m_FileDescriptor, buffer, size, 0 );

                if ( result == -1 ) {
                    if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
                        throw std::runtime_error( "Receive timed out" );
                    }

                    throw std::runtime_error( std::string( "Failed to receive UDP packet: " ) + std::strerror( errno ) );
                }

                return static_cast<std::size_t>( result );
            }

            bool WaitReadable( std::chrono::milliseconds timeout ) const override {
                if ( timeout.count() < 0 || timeout.count() > INT_MAX ) {
                    throw std::runtime_error( "Invalid UDP poll timeout" );
                }

                pollfd descriptors[2] {
                    { .fd = m_FileDescriptor, .events = POLLIN, .revents = 0 },
                    { .fd = m_WakeupDescriptor, .events = POLLIN, .revents = 0 },
                };

                while ( true ) {
                    const int result = ::poll( descriptors, 2, static_cast<int>( timeout.count() ) );

                    if ( result == 0 ) {
                        return false;
                    }

                    if ( result == -1 ) {
                        if ( errno == EINTR ) {
                            continue;
                        }

                        throw std::runtime_error( std::string( "Failed to poll UDP socket: " ) + std::strerror( errno ) );
                    }

                    for ( const pollfd& descriptor : descriptors ) {
                        if ( ( descriptor.revents & POLLNVAL ) != 0 ) {
                            throw std::runtime_error( "UDP poll descriptor became invalid" );
                        }

                        if ( ( descriptor.revents & POLLERR ) != 0 ) {
                            throw std::runtime_error( "UDP poll descriptor reported an error" );
                        }

                        if ( ( descriptor.revents & POLLHUP ) != 0 ) {
                            throw std::runtime_error( "UDP poll descriptor reported a hangup" );
                        }
                    }

                    if ( ( descriptors[1].revents & POLLIN ) != 0 ) {
                        std::uint64_t value = 0;

                        while ( true ) {
                            const ssize_t bytes = ::read( m_WakeupDescriptor, &value, sizeof( value ) );

                            if ( bytes == static_cast<ssize_t>( sizeof( value ) ) ) {
                                break;
                            }

                            if ( bytes == -1 && errno == EINTR ) {
                                continue;
                            }

                            if ( bytes == -1 && ( errno == EAGAIN || errno == EWOULDBLOCK ) ) {
                                break;
                            }

                            if ( bytes == -1 ) {
                                throw std::runtime_error( std::string( "Failed to drain UDP wakeup descriptor: " ) +
                                                          std::strerror( errno ) );
                            }

                            throw std::runtime_error( "Failed to read complete UDP wakeup value" );
                        }
                    }

                    return ( descriptors[0].revents & POLLIN ) != 0;
                }
            }

          private:
            Address m_Address;
            int m_FileDescriptor = -1;
            int m_WakeupDescriptor = -1;
        };

    } // namespace

    std::unique_ptr<SocketBackend> CreateSocketBackend( const Address& address ) {
        return std::make_unique<PosixSocketBackend>( address );
    }

} // namespace ts::net
