#include <cctype>
#include <charconv>
#include <client/cli/target_resolver.hpp>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace ts::client::cli {

    std::optional<ResolvedTarget> TargetResolver::ResolvePrivate( std::span<const TargetCandidate> candidates,
                                                                  std::string_view name ) {
        return Resolve( candidates, name, true, false );
    }

    std::optional<ResolvedTarget> TargetResolver::ResolveChannel( std::span<const TargetCandidate> candidates,
                                                                  std::string_view name ) {
        return Resolve( candidates, name, false, true );
    }

    UniqueTargetResolution TargetResolver::ResolveUniqueClient( std::span<const TargetCandidate> candidates,
                                                                std::string_view name ) {
        if ( name.size() > 1 && name.front() == '#' ) {
            std::uint64_t requestedId = 0;
            const char* first = name.data() + 1;
            const char* last = name.data() + name.size();
            const auto parsed = std::from_chars( first, last, requestedId );
            if ( parsed.ec == std::errc {} && parsed.ptr == last ) {
                for ( const TargetCandidate& candidate : candidates ) {
                    if ( candidate.kind == TargetKind::Client && candidate.id == requestedId ) {
                        return UniqueTargetResolution { .status = UniqueTargetStatus::Match,
                                                        .target = ResolvedTarget { .kind = candidate.kind,
                                                                                   .id = candidate.id,
                                                                                   .name = std::string( candidate.name ) } };
                    }
                }
                return {};
            }
        }

        const auto findMatches = [&candidates, name]( bool insensitive ) {
            std::vector<const TargetCandidate*> matches;
            for ( const TargetCandidate& candidate : candidates ) {
                if ( candidate.kind != TargetKind::Client ) {
                    continue;
                }
                const bool matchesName = insensitive ? EqualAsciiInsensitive( candidate.name, name ) : candidate.name == name;
                if ( matchesName ) {
                    matches.push_back( &candidate );
                }
            }
            return matches;
        };

        std::vector<const TargetCandidate*> matches = findMatches( false );
        if ( matches.empty() ) {
            matches = findMatches( true );
        }
        if ( matches.empty() ) {
            return {};
        }
        if ( matches.size() != 1 ) {
            return UniqueTargetResolution { .status = UniqueTargetStatus::Ambiguous, .target = std::nullopt };
        }
        const TargetCandidate& candidate = *matches.front();
        return UniqueTargetResolution {
            .status = UniqueTargetStatus::Match,
            .target = ResolvedTarget { .kind = candidate.kind, .id = candidate.id, .name = std::string( candidate.name ) } };
    }

    std::optional<ResolvedTarget> TargetResolver::Resolve( std::span<const TargetCandidate> candidates,
                                                           std::string_view name,
                                                           bool allowClients,
                                                           bool allowChannels ) {
        const auto allowed = [allowClients, allowChannels]( TargetKind kind ) {
            if ( kind == TargetKind::Client ) {
                return allowClients;
            }

            return allowChannels;
        };

        for ( const TargetCandidate& candidate : candidates ) {
            if ( allowed( candidate.kind ) && candidate.name == name ) {
                return ResolvedTarget { .kind = candidate.kind, .id = candidate.id, .name = std::string( candidate.name ) };
            }
        }

        for ( const TargetCandidate& candidate : candidates ) {
            if ( allowed( candidate.kind ) && EqualAsciiInsensitive( candidate.name, name ) ) {
                return ResolvedTarget { .kind = candidate.kind, .id = candidate.id, .name = std::string( candidate.name ) };
            }
        }

        return std::nullopt;
    }

    bool TargetResolver::EqualAsciiInsensitive( std::string_view left, std::string_view right ) {
        if ( left.size() != right.size() ) {
            return false;
        }

        for ( std::size_t index = 0; index < left.size(); ++index ) {
            const auto leftByte = static_cast<unsigned char>( left[index] );
            const auto rightByte = static_cast<unsigned char>( right[index] );

            if ( std::tolower( leftByte ) != std::tolower( rightByte ) ) {
                return false;
            }
        }

        return true;
    }

} // namespace ts::client::cli
