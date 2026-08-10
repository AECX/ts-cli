#ifndef TS_NET_SRV_RECORD_HPP
#define TS_NET_SRV_RECORD_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ts::net {

    struct SrvRecord {
        std::string target;
        std::uint16_t port = 0;
        std::uint16_t priority = 0;
        std::uint16_t weight = 0;
    };

    /*
     * Resolves "_service._proto.domain" (e.g. "_ts3", "_udp", "example.com")
     * to its preferred target: lowest priority, then highest weight among
     * ties (RFC 2782 selection without weighted-random tie-breaking, since
     * that only matters for load-balancing across multiple equal-priority
     * records, which real-world usage of this doesn't need).
     *
     * Never throws: an absent record, a resolver error, or a malformed
     * response all just yield std::nullopt. This is a best-effort lookup
     * layered in front of ordinary DNS resolution, not a hard requirement,
     * so a flaky or unsupported resolver must never block a connection.
     */
    [[nodiscard]] std::optional<SrvRecord>
        ResolveSrvRecord( std::string_view service, std::string_view proto, std::string_view domain );

} // namespace ts::net

#endif // TS_NET_SRV_RECORD_HPP
