#ifndef TS_CONFIG_PLATFORM_PATHS_HPP
#define TS_CONFIG_PLATFORM_PATHS_HPP

#include <filesystem>

namespace ts::config::platform {

    /*
     * The platform's per-user configuration root: $XDG_CONFIG_HOME (or
     * $HOME/.config) on POSIX, %APPDATA% on Windows. config::Paths::Discover
     * appends "ts-cli" to whatever this returns.
     */
    [[nodiscard]] std::filesystem::path ConfigHomeDirectory();

} // namespace ts::config::platform

#endif // TS_CONFIG_PLATFORM_PATHS_HPP
