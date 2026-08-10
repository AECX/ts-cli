#include "test_support.hpp"

#include <client/config/user_config.hpp>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace ts::client::test {

    void RunUserConfigTests() {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ( "ts-cli-user-config-test-" + UniqueTestDirectorySuffix() );
        std::filesystem::remove_all( root );
        std::filesystem::create_directories( root );

        try {
            UserConfigStore store( root );
            constexpr std::string_view UniqueId = "client/id+=example";

            const UserConfig initial = store.Load( UniqueId );
            Expect( UserConfigStore::IsDefault( initial ), "Missing remote user config did not use defaults" );

            const UserConfig configured { .volumeDb = -8.5F, .muted = true };
            store.Save( UniqueId, configured );

            const UserConfig loaded = store.Load( UniqueId );
            Expect( loaded.volumeDb == configured.volumeDb, "Remote user volume was not persisted" );
            Expect( loaded.muted, "Remote user mute was not persisted" );

            std::filesystem::path path;
            for ( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( root ) ) {
                path = entry.path();
            }
            Expect( !path.empty() && std::filesystem::exists( path ), "Remote user config was not created" );
            Expect( path.filename().string().find( '/' ) == std::string::npos,
                    "Remote identity was not encoded as a safe filename" );

#ifndef _WIN32
            /* Raw POSIX mode bits aren't meaningful on Windows; see client/platform/secure_file.hpp. */
            struct stat information {};
            Expect( ::stat( path.c_str(), &information ) == 0, "Could not stat remote user config" );
            Expect( ( information.st_mode & 0777 ) == 0600, "Remote user config permissions are not 0600" );
#endif

            store.Save( UniqueId, UserConfig {} );
            Expect( !std::filesystem::exists( path ), "Default remote user settings left an unnecessary file" );

            bool rejectedRange = false;
            try {
                store.Save( "uid", UserConfig { .volumeDb = 13.0F, .muted = false } );
            } catch ( const std::runtime_error& ) {
                rejectedRange = true;
            }
            Expect( rejectedRange, "Out-of-range remote user volume was accepted" );

            store.Save( "mismatch", UserConfig { .volumeDb = -1.0F, .muted = false } );
            std::filesystem::path mismatched;
            for ( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( root ) ) {
                mismatched = entry.path();
            }
            {
                std::ofstream stream( mismatched, std::ios::trunc );
                stream << "identity_uid=somebody-else\nvolume_db=-1\nmuted=false\n";
            }
            bool rejectedMismatch = false;
            try {
                (void)store.Load( "mismatch" );
            } catch ( const std::runtime_error& ) {
                rejectedMismatch = true;
            }
            Expect( rejectedMismatch, "Remote user config with a mismatched identity was accepted" );
        } catch ( ... ) {
            std::filesystem::remove_all( root );
            throw;
        }

        std::filesystem::remove_all( root );
    }

} // namespace ts::client::test
