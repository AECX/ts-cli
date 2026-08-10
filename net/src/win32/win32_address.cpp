#include "../host_resolution.hpp"
#include "win32_error.hpp"
#include "win32_winsock.hpp"

#include <cstring>
#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace ts::net {

    namespace {

        static_assert( sizeof( SOCKADDR_STORAGE ) <= Address::StorageSize,
                       "Address storage is too small to hold a SOCKADDR_STORAGE on this platform" );

        const sockaddr* AsSockaddr( const Address& address ) {
            return reinterpret_cast<const sockaddr*>( address.storage.data() );
        }

    } // namespace

    std::vector<Address> ResolveHostPort( const Endpoint& endpoint ) {
        EnsureWinsockInitialized();

        addrinfo hints {};

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        const std::string port = std::to_string( endpoint.port );

        addrinfo* results = nullptr;

        const int status = ::getaddrinfo( endpoint.host.c_str(), port.c_str(), &hints, &results );

        if ( status != 0 ) {
            throw std::runtime_error( std::string( "Failed to resolve endpoint: " ) + FormatWsaError( status ) );
        }

        std::vector<Address> addresses;
        for ( addrinfo* current = results; current != nullptr; current = current->ai_next ) {
            Address address {};

            ::memcpy( address.storage.data(), current->ai_addr, static_cast<std::size_t>( current->ai_addrlen ) );
            address.length = static_cast<std::size_t>( current->ai_addrlen );
            addresses.push_back( address );
        }

        ::freeaddrinfo( results );
        return addresses;
    }

    std::string FormatAddress( const Address& address ) {
        EnsureWinsockInitialized();

        char host[NI_MAXHOST];
        char service[NI_MAXSERV];

        const int status = ::getnameinfo( AsSockaddr( address ),
                                          static_cast<socklen_t>( address.length ),
                                          host,
                                          sizeof( host ),
                                          service,
                                          sizeof( service ),
                                          NI_NUMERICHOST | NI_NUMERICSERV );

        if ( status != 0 ) {
            throw std::runtime_error( std::string( "Failed to format address: " ) + FormatWsaError( status ) );
        }

        return std::string( host ) + ":" + std::string( service );
    }

    std::string FormatHost( const Address& address ) {
        EnsureWinsockInitialized();

        char host[NI_MAXHOST] {};

        const int result = ::getnameinfo( AsSockaddr( address ),
                                          static_cast<socklen_t>( address.length ),
                                          host,
                                          sizeof( host ),
                                          nullptr,
                                          0,
                                          NI_NUMERICHOST );

        if ( result != 0 ) {
            throw std::runtime_error( FormatWsaError( result ) );
        }

        return host;
    }
} // namespace ts::net
