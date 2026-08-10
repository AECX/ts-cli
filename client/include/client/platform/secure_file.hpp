#ifndef TS_CLIENT_PLATFORM_SECURE_FILE_HPP
#define TS_CLIENT_PLATFORM_SECURE_FILE_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ts::client::platform {

    /*
     * Reads a private, regular file capped at maxSize bytes, returning
     * std::nullopt if it does not exist. On POSIX this also actively
     * re-tightens the file's permissions to owner-only on every read; see
     * WriteSecureFile for why. Throws on any other failure, including a
     * file larger than maxSize or one that isn't a regular file.
     */
    [[nodiscard]] std::optional<std::string> ReadSecureFile( const std::filesystem::path& path, std::uint64_t maxSize );

    /*
     * Atomically (over)writes a private file: written to a sibling
     * temporary file first, then renamed into place, so a crash or
     * concurrent reader never observes a partial write. On POSIX the
     * temporary and final file are both kept owner-only (0600); on
     * Windows this relies on the per-user profile directory's default ACL
     * instead of an explicit DACL (see CONTRIBUTING.md).
     */
    void WriteSecureFile( const std::filesystem::path& path, std::string_view data );

    /*
     * Restricts an already-created directory to its owner. A no-op on
     * Windows for the same reason WriteSecureFile doesn't set a DACL
     * there: the profile directory it lives under is already private.
     */
    void SecureDirectory( const std::filesystem::path& path );

} // namespace ts::client::platform

#endif // TS_CLIENT_PLATFORM_SECURE_FILE_HPP
