#include "test_support.hpp"

#include <cstddef>
#include <protocol/crypto/p256.hpp>
#include <protocol/identity.hpp>
#include <stdexcept>
#include <string>

namespace ts::test {

    void RunIdentityTests() {
        {
            protocol::Identity original;

            const auto originalPublicKey = original.PublicKey();

            const std::string pem = original.PrivateKeyPem();

            protocol::Identity restored = protocol::Identity::FromPrivateKeyPem( pem );

            ExpectEqual( restored.PublicKey(), originalPublicKey, "Reloaded identity has a different public key" );

            const auto data = Bytes( "persistent identity test" );

            const auto signature = restored.Sign( data );

            Expect( protocol::P256::Verify( restored.PublicKey(), data, signature ),
                    "Reloaded identity produced an invalid signature" );

            /* base64 of a 20-byte SHA-1 digest, stable for the same key. */
            ExpectEqual( original.UniqueId().size(), std::size_t { 28 }, "Unique ID has the wrong length" );
            ExpectEqual( restored.UniqueId(), original.UniqueId(), "Reloaded identity has a different unique ID" );
            Expect( protocol::Identity {}.UniqueId() != original.UniqueId(), "Two identities share a unique ID" );
        }

        ExpectThrows<std::runtime_error>(
            []() {
                (void)protocol::Identity::FromPrivateKeyPem( "this is not a private key" );
            },
            "Invalid identity PEM was accepted" );
    }

} // namespace ts::test
