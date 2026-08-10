#ifndef TS_NET_ENDPOINT_HPP
#define TS_NET_ENDPOINT_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace ts::net {

    struct Endpoint {
        std::string host;
        std::uint16_t port;
    };

    Endpoint ParseEndpoint( std::string_view input );

} // namespace ts::net

#endif // TS_NET_ENDPOINT_HPP
