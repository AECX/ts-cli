#ifndef TS_CLIENT_CONFIG_IDENTITY_STORE_HPP
#define TS_CLIENT_CONFIG_IDENTITY_STORE_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <protocol/identity.hpp>
#include <string>

namespace ts::client {

    struct LegacyIdentityConfig {
        std::uint8_t securityLevel = 8;
        std::string encodedPrivateKey;
        std::optional<std::uint64_t> keyOffset;
    };

    struct LocalIdentity {
        protocol::Identity identity;
        std::uint8_t securityLevel = 8;
        std::uint64_t keyOffset = 0;
    };

    class IdentityStore {
      public:
        [[nodiscard]] static std::uint8_t DefaultSecurityLevel();

        [[nodiscard]] static LocalIdentity Create( protocol::Identity identity, std::uint8_t securityLevel );
        [[nodiscard]] static LocalIdentity Load( const std::filesystem::path& path );
        [[nodiscard]] static LocalIdentity LoadOrMigrate( const std::filesystem::path& path,
                                                          const LegacyIdentityConfig& legacy );

        static void Save( const std::filesystem::path& path, const LocalIdentity& identity );

      private:
        [[nodiscard]] static protocol::Identity DecodeLegacyIdentity( const std::string& encodedPrivateKey );
        [[nodiscard]] static std::string EncodeIdentity( const protocol::Identity& identity );
    };

} // namespace ts::client

#endif // TS_CLIENT_CONFIG_IDENTITY_STORE_HPP
