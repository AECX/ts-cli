#include "host_resolution.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <net/address.hpp>
#include <net/endpoint.hpp>
#include <net/srv_record.hpp>
#include <string>
#include <thread>
#include <vector>

namespace ts::net {

    namespace {

        /*
         * Neither platform's ResolveSrvRecord() backend can be given a
         * resolver-level timeout (res_query()/DnsQuery_A() just inherit
         * whatever retry/timeout policy the OS resolver defaults to, which
         * can be many seconds when a resolver is slow or silently drops an
         * uncommon record type like SRV). Since most hosts have no
         * _ts3._udp record at all, that cost would be paid on nearly every
         * connection. Run the lookup on a detached thread and give up on it
         * after a short budget instead, so a slow/unresponsive resolver can
         * never stall the connection past this point; the abandoned lookup
         * (if any) simply finishes in the background and is discarded.
         */
        constexpr std::chrono::milliseconds SrvLookupTimeout { 750 };

        std::optional<SrvRecord> ResolveSrvRecordWithTimeout( std::string_view domain ) {
            auto promise = std::make_shared<std::promise<std::optional<SrvRecord>>>();
            std::future<std::optional<SrvRecord>> future = promise->get_future();

            std::thread( [promise, host = std::string( domain )] {
                promise->set_value( ResolveSrvRecord( "_ts3", "_udp", host ) );
            } ).detach();

            if ( future.wait_for( SrvLookupTimeout ) != std::future_status::ready ) {
                return std::nullopt;
            }

            return future.get();
        }

    } // namespace

    std::vector<Address> ResolveEndpoint( const Endpoint& endpoint ) {
        if ( const auto srv = ResolveSrvRecordWithTimeout( endpoint.host ) ) {
            return ResolveHostPort( Endpoint { .host = srv->target, .port = srv->port } );
        }

        return ResolveHostPort( endpoint );
    }

} // namespace ts::net
