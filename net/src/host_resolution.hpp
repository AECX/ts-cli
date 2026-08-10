#ifndef TS_NET_HOST_RESOLUTION_HPP
#define TS_NET_HOST_RESOLUTION_HPP

#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <vector>

namespace ts::net {

    /*
     * Platform primitive: resolves a literal host:port via the OS resolver
     * (getaddrinfo/DnsQuery A/AAAA lookup), with no SRV involvement. Private
     * to net; the public net::ResolveEndpoint (endpoint_resolution.cpp)
     * tries an _ts3._udp SRV record first and falls back to this.
     */
    [[nodiscard]] std::vector<Address> ResolveHostPort( const Endpoint& endpoint );

} // namespace ts::net

#endif // TS_NET_HOST_RESOLUTION_HPP
