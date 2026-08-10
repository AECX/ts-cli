#include <client/config/paths.hpp>
#include <client/platform/paths.hpp>
#include <client/platform/secure_file.hpp>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace ts::client {

    Paths::Paths( std::filesystem::path configDirectory ): m_ConfigDirectory( std::move( configDirectory ) ) {
    }

    Paths Paths::Discover() {
        return FromConfigHome( platform::ConfigHomeDirectory() );
    }

    Paths Paths::FromConfigHome( std::filesystem::path configHome ) {
        if ( !configHome.is_absolute() ) {
            throw std::runtime_error( "Config home must be absolute" );
        }

        return Paths( std::move( configHome ) / "ts-cli" );
    }

    void Paths::EnsureDirectories() const {
        EnsureDirectory( m_ConfigDirectory );
        EnsureDirectory( UsersDirectory() );
    }

    const std::filesystem::path& Paths::ConfigDirectory() const {
        return m_ConfigDirectory;
    }

    std::filesystem::path Paths::ConfigFile() const {
        return m_ConfigDirectory / "config.conf";
    }

    std::filesystem::path Paths::IdentityFile() const {
        return m_ConfigDirectory / "identity";
    }

    std::filesystem::path Paths::UsersDirectory() const {
        return m_ConfigDirectory / "users";
    }

    void Paths::EnsureDirectory( const std::filesystem::path& path ) {
        std::error_code error;

        std::filesystem::create_directories( path, error );

        if ( error ) {
            throw std::runtime_error( "Failed to create config directory: " + path.string() + ": " + error.message() );
        }

        if ( !std::filesystem::is_directory( path, error ) || error ) {
            throw std::runtime_error( "Config path is not a directory: " + path.string() );
        }

        platform::SecureDirectory( path );
    }

} // namespace ts::client
