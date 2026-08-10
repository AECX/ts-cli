#ifndef TS_CLIENT_PLATFORM_PATHS_HPP
#define TS_CLIENT_PLATFORM_PATHS_HPP

#include <filesystem>

namespace ts::client::platform {

    /*
     * The platform's per-user configuration root: $XDG_CONFIG_HOME (or
     * $HOME/.config) on POSIX, %APPDATA% on Windows. client::Paths::Discover
     * appends "ts-cli" to whatever this returns.
     */
    [[nodiscard]] std::filesystem::path ConfigHomeDirectory();

} // namespace ts::client::platform

#endif // TS_CLIENT_PLATFORM_PATHS_HPP
