#include <client/platform/paths.hpp>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace ts::client::platform {

    std::filesystem::path ConfigHomeDirectory() {
        const char* value = std::getenv( "APPDATA" );

        if ( value == nullptr || *value == '\0' ) {
            throw std::runtime_error( "APPDATA is not set" );
        }

        std::filesystem::path path( value );

        if ( !path.is_absolute() ) {
            throw std::runtime_error( "APPDATA is not set to an absolute path" );
        }

        return path;
    }

} // namespace ts::client::platform
