#include <net/srv_record.hpp>
#include <string>

// clang-format off
#include <windows.h>
#include <windns.h>
// clang-format on

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

        PDNS_RECORD records = nullptr;
        const DNS_STATUS status = ::DnsQuery_A( query.c_str(), DNS_TYPE_SRV, DNS_QUERY_STANDARD, nullptr, &records, nullptr );

        if ( status != 0 || records == nullptr ) {
            return std::nullopt;
        }

        std::optional<SrvRecord> best;

        for ( PDNS_RECORD current = records; current != nullptr; current = current->pNext ) {
            if ( current->wType != DNS_TYPE_SRV ) {
                continue;
            }

            SrvRecord candidate;
            candidate.priority = current->Data.SRV.wPriority;
            candidate.weight = current->Data.SRV.wWeight;
            candidate.port = current->Data.SRV.wPort;
            candidate.target = current->Data.SRV.pNameTarget;

            if ( !best || IsBetter( candidate, *best ) ) {
                best = candidate;
            }
        }

        ::DnsRecordListFree( records, DnsFreeRecordList );

        return best;
    }

} // namespace ts::net
