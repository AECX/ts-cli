#include "test_support.hpp"

#include <array>
#include <client/cli/target_resolver.hpp>
#include <cstdint>
#include <string>

namespace ts::client::test {

    void RunTargetResolverTests() {
        const std::array<cli::TargetCandidate, 5> candidates = {
            cli::TargetCandidate { .kind = cli::TargetKind::Client, .id = 4, .name = "Lobby" },
            cli::TargetCandidate { .kind = cli::TargetKind::Client, .id = 11, .name = "User" },
            cli::TargetCandidate { .kind = cli::TargetKind::Client, .id = 12, .name = "User" },
            cli::TargetCandidate { .kind = cli::TargetKind::Channel, .id = 1, .name = "Lobby" },
            cli::TargetCandidate { .kind = cli::TargetKind::Channel, .id = 7, .name = "Konferenzraum 2" } };

        {
            const auto target = cli::TargetResolver::ResolvePrivate( candidates, "Lobby" );
            Expect( target.has_value(), "/pm-style resolution did not find the client" );
            Expect( target->kind == cli::TargetKind::Client, "Private resolution selected a channel" );
            ExpectEqual( target->id, std::uint64_t { 4 }, "Private resolution selected the wrong client" );
        }

        {
            const auto target = cli::TargetResolver::ResolveChannel( candidates, "Lobby" );
            Expect( target.has_value(), "Channel-only resolution found no target" );
            Expect( target->kind == cli::TargetKind::Channel, "Channel-only resolution selected a client" );
            ExpectEqual( target->id, std::uint64_t { 1 }, "Channel-only resolution selected the wrong channel" );
        }

        {
            const auto target = cli::TargetResolver::ResolvePrivate( candidates, "User" );
            Expect( target.has_value(), "Duplicate client resolution found no target" );
            ExpectEqual( target->id, std::uint64_t { 11 }, "Duplicate client resolution did not preserve first-match order" );
        }

        {
            const auto resolution = cli::TargetResolver::ResolveUniqueClient( candidates, "User" );
            Expect( resolution.status == cli::UniqueTargetStatus::Ambiguous,
                    "Persistent user target resolution accepted an ambiguous nickname" );
            Expect( !resolution.target.has_value(), "Ambiguous persistent user target returned a client" );
        }

        {
            const auto resolution = cli::TargetResolver::ResolveUniqueClient( candidates, "#12" );
            Expect( resolution.status == cli::UniqueTargetStatus::Match && resolution.target.has_value(),
                    "Persistent user target resolution did not accept #<client-id>" );
            ExpectEqual( resolution.target->id,
                         std::uint64_t { 12 },
                         "Persistent user target resolution selected the wrong client ID" );
        }

        {
            const auto target = cli::TargetResolver::ResolveChannel( candidates, "lobby" );
            Expect( target.has_value(), "Case-insensitive fallback found no channel" );
            ExpectEqual( target->id, std::uint64_t { 1 }, "Case-insensitive fallback selected the wrong channel" );
        }
    }

} // namespace ts::client::test
