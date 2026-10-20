#ifndef TS_CONFIG_IDENTITY_STORE_HPP
#define TS_CONFIG_IDENTITY_STORE_HPP

#include <cstdint>
#include <filesystem>
#include <protocol/identity.hpp>
#include <string>

namespace ts::config {

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

        static void Save( const std::filesystem::path& path, const LocalIdentity& identity );

      private:
        [[nodiscard]] static protocol::Identity DecodeIdentity( const std::string& encodedPrivateKey );
        [[nodiscard]] static std::string EncodeIdentity( const protocol::Identity& identity );
    };

} // namespace ts::config

#endif // TS_CONFIG_IDENTITY_STORE_HPP
