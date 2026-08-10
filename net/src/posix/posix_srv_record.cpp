#include <arpa/nameser.h>
#include <net/srv_record.hpp>
#include <resolv.h>
#include <string>

namespace ts::net {

    namespace {

        bool IsBetter( const SrvRecord& candidate, const SrvRecord& best ) {
            if ( candidate.priority != best.priority ) {
                return candidate.priority < best.priority;
            }

            return candidate.weight > best.weight;
        }

    } // namespace

    std::optional<SrvRecord> ResolveSrvRecord( std::string_view service, std::string_view proto, std::string_view domain ) {
        const std::string query = std::string( service ) + "." + std::string( proto ) + "." + std::string( domain );

        unsigned char answer[NS_PACKETSZ * 4];
        const int length = res_query( query.c_str(), ns_c_in, ns_t_srv, answer, sizeof( answer ) );

        if ( length <= 0 ) {
            return std::nullopt;
        }

        ns_msg handle;

        if ( ns_initparse( answer, length, &handle ) != 0 ) {
            return std::nullopt;
        }

        const int count = ns_msg_count( handle, ns_s_an );

        std::optional<SrvRecord> best;

        for ( int index = 0; index < count; ++index ) {
            ns_rr record;

            if ( ns_parserr( &handle, ns_s_an, index, &record ) != 0 || ns_rr_type( record ) != ns_t_srv ||
                 ns_rr_rdlen( record ) < 6 ) {
                continue;
            }

            const unsigned char* data = ns_rr_rdata( record );

            SrvRecord candidate;
            candidate.priority = ns_get16( data );
            candidate.weight = ns_get16( data + 2 );
            candidate.port = ns_get16( data + 4 );

            char target[NS_MAXDNAME];
            const int targetLength =
                dn_expand( ns_msg_base( handle ), ns_msg_end( handle ), data + 6, target, sizeof( target ) );

            if ( targetLength < 0 ) {
                continue;
            }

            candidate.target = target;

            if ( !best || IsBetter( candidate, *best ) ) {
                best = candidate;
            }
        }

        return best;
    }

} // namespace ts::net
