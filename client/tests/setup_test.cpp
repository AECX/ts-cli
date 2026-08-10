#include "test_support.hpp"

#include <client/config/config.hpp>
#include <client/config/identity_store.hpp>
#include <client/config/paths.hpp>
#include <client/config/setup.hpp>
#include <filesystem>
#include <sstream>
#include <string>

namespace ts::client::test {

    void RunSetupTests() {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ( "ts-cli-setup-test-" + UniqueTestDirectorySuffix() );
        std::filesystem::remove_all( root );

        try {
            const Paths paths = Paths::FromConfigHome( root / "config" );
            paths.EnsureDirectories();

            std::istringstream input( "uhinf\n"
                                      "n\n"
                                      "0x12345678\n"
                                      "ts-cli experimental\n"
                                      "Arch Linux\n"
                                      "-\n"
                                      "2\n"
                                      "y\n" );
            std::ostringstream output;

            const Config config = ConfigSetup::Run( paths.ConfigFile(), paths.IdentityFile(), input, output );
            const protocol::ClientProfile profile = config.Profile();
            ExpectEqual( profile.nickname, std::string( "uhinf" ), "Setup nickname is incorrect" );
            ExpectEqual( profile.version.initVersion, std::uint32_t { 0x12345678 }, "Setup Init1 version is incorrect" );
            ExpectEqual( profile.version.version, std::string( "ts-cli experimental" ), "Setup client version is incorrect" );
            ExpectEqual( profile.version.platform, std::string( "Arch Linux" ), "Setup platform is incorrect" );
            Expect( profile.version.signature.empty(), "Setup did not allow an empty signature" );
            Expect( std::filesystem::exists( paths.ConfigFile() ), "Setup did not create config" );
            Expect( std::filesystem::exists( paths.IdentityFile() ), "Setup did not create identity file" );

            LocalIdentity identity = IdentityStore::Load( paths.IdentityFile() );
            const std::vector<std::byte> publicKey = identity.identity.PublicKey();
            Expect( !publicKey.empty(), "Setup did not create a valid identity" );
            ExpectEqual( identity.securityLevel, std::uint8_t { 2 }, "Setup security level is incorrect" );

            const Config reloaded = Config::Load( paths.ConfigFile() );
            ExpectEqual( reloaded.Profile().nickname, profile.nickname, "Reloaded setup nickname changed" );
            Expect( !reloaded.HasLegacyIdentity(), "Fresh setup wrote legacy identity fields to config" );
            Expect( IdentityStore::Load( paths.IdentityFile() ).identity.PublicKey() == publicKey,
                    "Reloaded setup identity changed" );

            std::filesystem::remove( paths.ConfigFile() );
            std::istringstream recoveryInput( "recovered\n"
                                              "y\n" );
            std::ostringstream recoveryOutput;
            const Config recovered =
                ConfigSetup::Run( paths.ConfigFile(), paths.IdentityFile(), recoveryInput, recoveryOutput );
            ExpectEqual( recovered.Profile().nickname,
                         std::string( "recovered" ),
                         "Setup recovery did not recreate client config" );
            Expect( IdentityStore::Load( paths.IdentityFile() ).identity.PublicKey() == publicKey,
                    "Setup recovery replaced an existing local identity" );
        } catch ( ... ) {
            std::filesystem::remove_all( root );
            throw;
        }

        std::filesystem::remove_all( root );
    }

} // namespace ts::client::test
