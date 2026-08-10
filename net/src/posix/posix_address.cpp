#include "../host_resolution.hpp"

#include <cstring>
#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <vector>

namespace ts::net {

    namespace {

        static_assert( sizeof( sockaddr_storage ) <= Address::StorageSize,
                       "Address storage is too small to hold a sockaddr_storage on this platform" );

        const sockaddr* AsSockaddr( const Address& address ) {
            return reinterpret_cast<const sockaddr*>( address.storage.data() );
        }

    } // namespace

    std::vector<Address> ResolveHostPort( const Endpoint& endpoint ) {
        addrinfo hints {};

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        const std::string port = std::to_string( endpoint.port );

        addrinfo* results = nullptr;

        const int status = getaddrinfo( endpoint.host.c_str(), port.c_str(), &hints, &results );

        if ( status != 0 ) {
            throw std::runtime_error( std::string( "Failed to resolve endpoint: " ) + gai_strerror( status ) );
        }

        std::vector<Address> addresses;
        for ( addrinfo* current = results; current != nullptr; current = current->ai_next ) {
            Address address {};

            ::memcpy( address.storage.data(), current->ai_addr, current->ai_addrlen );
            address.length = current->ai_addrlen;
            addresses.push_back( address );
        }

        freeaddrinfo( results );
        return addresses;
    }

    std::string FormatAddress( const Address& address ) {
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];

        const int status = getnameinfo( AsSockaddr( address ),
                                        static_cast<socklen_t>( address.length ),
                                        host,
                                        sizeof( host ),
                                        service,
                                        sizeof( service ),
                                        NI_NUMERICHOST | NI_NUMERICSERV );

        if ( status != 0 ) {
            throw std::runtime_error( std::string( "Failed to format address: " ) + gai_strerror( status ) );
        }

        return std::string( host ) + ":" + std::string( service );
    }

    std::string FormatHost( const Address& address ) {
        char host[NI_MAXHOST] {};

        const int result = ::getnameinfo( AsSockaddr( address ),
                                          static_cast<socklen_t>( address.length ),
                                          host,
                                          sizeof( host ),
                                          nullptr,
                                          0,
                                          NI_NUMERICHOST );

        if ( result != 0 ) {
            throw std::runtime_error( ::gai_strerror( result ) );
        }

        return host;
    }
} // namespace ts::net
