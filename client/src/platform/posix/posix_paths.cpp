#include <client/platform/paths.hpp>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>

namespace ts::client::platform {

    namespace {

        [[nodiscard]] std::optional<std::filesystem::path> AbsoluteEnvironmentPath( const char* name ) {
            const char* value = std::getenv( name );

            if ( value == nullptr || *value == '\0' ) {
                return std::nullopt;
            }

            std::filesystem::path path( value );

            if ( !path.is_absolute() ) {
                return std::nullopt;
            }

            return path;
        }

    } // namespace

    std::filesystem::path ConfigHomeDirectory() {
        if ( const auto configHome = AbsoluteEnvironmentPath( "XDG_CONFIG_HOME" ) ) {
            return *configHome;
        }

        const auto home = AbsoluteEnvironmentPath( "HOME" );

        if ( !home ) {
            throw std::runtime_error( "HOME is not set to an absolute path" );
        }

        return *home / ".config";
    }

} // namespace ts::client::platform
