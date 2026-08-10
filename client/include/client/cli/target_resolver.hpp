#ifndef TS_CLIENT_CLI_TARGET_RESOLVER_HPP
#define TS_CLIENT_CLI_TARGET_RESOLVER_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ts::client::cli {

    enum class TargetKind { Client, Channel };

    struct TargetCandidate {
        TargetKind kind = TargetKind::Client;
        std::uint64_t id = 0;
        std::string_view name;
    };

    struct ResolvedTarget {
        TargetKind kind = TargetKind::Client;
        std::uint64_t id = 0;
        std::string name;
    };

    enum class UniqueTargetStatus { NoMatch, Match, Ambiguous };

    struct UniqueTargetResolution {
        UniqueTargetStatus status = UniqueTargetStatus::NoMatch;
        std::optional<ResolvedTarget> target;
    };

    class TargetResolver {
      public:
        [[nodiscard]] static std::optional<ResolvedTarget> ResolvePrivate( std::span<const TargetCandidate> candidates,
                                                                           std::string_view name );

        [[nodiscard]] static std::optional<ResolvedTarget> ResolveChannel( std::span<const TargetCandidate> candidates,
                                                                           std::string_view name );

        [[nodiscard]] static UniqueTargetResolution ResolveUniqueClient( std::span<const TargetCandidate> candidates,
                                                                         std::string_view name );

      private:
        [[nodiscard]] static std::optional<ResolvedTarget> Resolve( std::span<const TargetCandidate> candidates,
                                                                    std::string_view name,
                                                                    bool allowClients,
                                                                    bool allowChannels );

        [[nodiscard]] static bool EqualAsciiInsensitive( std::string_view left, std::string_view right );
    };

} // namespace ts::client::cli

#endif // TS_CLIENT_CLI_TARGET_RESOLVER_HPP
