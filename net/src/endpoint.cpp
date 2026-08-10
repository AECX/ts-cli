#include <cstdint>
#include <limits>
#include <net/endpoint.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ts::net {

    Endpoint ParseEndpoint( std::string_view input ) {
        const std::size_t separator = input.rfind( ':' );

        if ( separator == std::string_view::npos ) {
            return Endpoint { .host = std::string( input ), .port = 9987 };
        }

        if ( separator == 0 ) {
            throw std::invalid_argument( "Endpoint is missing host" );
        }

        if ( separator == input.size() - 1 ) {
            throw std::invalid_argument( "Endpoint is missing port" );
        }

        const std::string_view host = input.substr( 0, separator );
        const std::string_view port_string = input.substr( separator + 1 );

        unsigned long port;
        std::size_t characters_processed;

        try {
            port = std::stoul( std::string( port_string ), &characters_processed );
        } catch ( const std::invalid_argument& ) {
            throw std::invalid_argument( "Endpoint port is not a number" );
        } catch ( const std::out_of_range& ) {
            throw std::invalid_argument( "Endpoint port is out of range" );
        }

        if ( characters_processed != port_string.size() ) {
            throw std::invalid_argument( "Endpoint port is not a number" );
        }

        if ( port > std::numeric_limits<std::uint16_t>::max() ) {
            throw std::invalid_argument( "Endpoint port is out of range" );
        }

        return Endpoint { .host = std::string( host ), .port = static_cast<std::uint16_t>( port ) };
    }
} // namespace ts::net
