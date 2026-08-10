#ifndef TS_NET_ADDRESS_HPP
#define TS_NET_ADDRESS_HPP

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace ts::net {

    /*
     * Opaque, platform-agnostic storage for a resolved socket address.
     * Backends reinterpret the storage as their native sockaddr type;
     * the public API never exposes OS socket headers, so callers can
     * hold and pass addresses around without depending on a target's
     * networking headers.
     */
    struct Address {
        static constexpr std::size_t StorageSize = 128;

        alignas( std::max_align_t ) std::array<std::byte, StorageSize> storage {};
        std::size_t length = 0;
    };

    struct Endpoint;

    [[nodiscard]] std::vector<Address> ResolveEndpoint( const Endpoint& endpoint );

    [[nodiscard]] std::string FormatHost( const Address& address );
    [[nodiscard]] std::string FormatAddress( const Address& address );

} // namespace ts::net

#endif // TS_NET_ADDRESS_HPP
