#ifndef TS_CONFIG_IDENTITY_STORE_HPP
#define TS_CONFIG_IDENTITY_STORE_HPP

#include <cstdint>
#include <filesystem>
#include <protocol/crypto/hash_cash.hpp>
#include <protocol/identity.hpp>
#include <stop_token>
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
        [[nodiscard]] static std::uint8_t MaxSecurityLevel();

        [[nodiscard]] static LocalIdentity Create( protocol::Identity identity, std::uint8_t securityLevel );
        [[nodiscard]] static LocalIdentity Load( const std::filesystem::path& path );

        static void Save( const std::filesystem::path& path, const LocalIdentity& identity );

        /*
         * Raises the identity's security level to `targetLevel` by searching
         * for a new key offset, continuing from the current one (no smaller
         * offset reaches the current level, let alone a higher one). The key
         * itself, and so the unique ID, stays the same. Returns false, with
         * `identity` unchanged, if `stop` was requested first. Each level
         * doubles the expected time; this is meant to run on a worker thread.
         */
        [[nodiscard]] static bool IncreaseSecurityLevel( LocalIdentity& identity,
                                                         std::uint8_t targetLevel,
                                                         const std::stop_token& stop,
                                                         const protocol::HashCash::ProgressCallback& progress = {} );

        /* Reads a private key PEM file, e.g. one exported from another client. */
        [[nodiscard]] static protocol::Identity ImportPem( const std::filesystem::path& path );

        /* Writes the private key as PEM, owner-only like the identity file itself. */
        static void ExportPem( const std::filesystem::path& path, const protocol::Identity& identity );

      private:
        [[nodiscard]] static protocol::Identity DecodeIdentity( const std::string& encodedPrivateKey );
        [[nodiscard]] static std::string EncodeIdentity( const protocol::Identity& identity );
    };

} // namespace ts::config

#endif // TS_CONFIG_IDENTITY_STORE_HPP
