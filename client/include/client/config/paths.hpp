#ifndef TS_CLIENT_CONFIG_PATHS_HPP
#define TS_CLIENT_CONFIG_PATHS_HPP

#include <filesystem>

namespace ts::client {

    class Paths {
      public:
        [[nodiscard]] static Paths Discover();

        [[nodiscard]] static Paths FromConfigHome( std::filesystem::path configHome );

        void EnsureDirectories() const;

        [[nodiscard]] const std::filesystem::path& ConfigDirectory() const;

        [[nodiscard]] std::filesystem::path ConfigFile() const;
        [[nodiscard]] std::filesystem::path IdentityFile() const;
        [[nodiscard]] std::filesystem::path UsersDirectory() const;

      private:
        explicit Paths( std::filesystem::path configDirectory );

        static void EnsureDirectory( const std::filesystem::path& path );

        std::filesystem::path m_ConfigDirectory;
    };

} // namespace ts::client

#endif // TS_CLIENT_CONFIG_PATHS_HPP
