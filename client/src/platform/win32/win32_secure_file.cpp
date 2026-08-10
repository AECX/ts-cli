#include <client/platform/secure_file.hpp>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <windows.h>

namespace ts::client::platform {

    namespace {

        std::string FormatWin32Error( DWORD code ) {
            char buffer[256] {};

            const DWORD length = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                                   nullptr,
                                                   code,
                                                   0,
                                                   buffer,
                                                   sizeof( buffer ),
                                                   nullptr );

            if ( length == 0 ) {
                return "Win32 error " + std::to_string( code );
            }

            std::string message( buffer, length );

            while ( !message.empty() && ( message.back() == '\n' || message.back() == '\r' ) ) {
                message.pop_back();
            }

            return message;
        }

        class FileHandle {
          public:
            explicit FileHandle( HANDLE handle ): m_Handle( handle ) {
            }

            ~FileHandle() {
                if ( m_Handle != INVALID_HANDLE_VALUE ) {
                    ::CloseHandle( m_Handle );
                }
            }

            FileHandle( const FileHandle& ) = delete;
            FileHandle& operator=( const FileHandle& ) = delete;

            [[nodiscard]] HANDLE Get() const {
                return m_Handle;
            }

          private:
            HANDLE m_Handle;
        };

        std::runtime_error SystemError( std::string_view action, const std::filesystem::path& path, DWORD error ) {
            return std::runtime_error( std::string( action ) + ": " + path.string() + ": " + FormatWin32Error( error ) );
        }

    } // namespace

    std::optional<std::string> ReadSecureFile( const std::filesystem::path& path, std::uint64_t maxSize ) {
        const HANDLE handle = ::CreateFileW( path.c_str(),
                                             GENERIC_READ,
                                             FILE_SHARE_READ,
                                             nullptr,
                                             OPEN_EXISTING,
                                             FILE_ATTRIBUTE_NORMAL,
                                             nullptr );

        if ( handle == INVALID_HANDLE_VALUE ) {
            const DWORD error = ::GetLastError();

            if ( error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ) {
                return std::nullopt;
            }

            throw SystemError( "Failed to open secure file", path, error );
        }

        FileHandle file( handle );
        LARGE_INTEGER size {};

        if ( ::GetFileSizeEx( file.Get(), &size ) == 0 ) {
            throw SystemError( "Failed to inspect secure file", path, ::GetLastError() );
        }

        if ( size.QuadPart < 0 || static_cast<std::uint64_t>( size.QuadPart ) > maxSize ) {
            throw std::runtime_error( "Secure file is too large: " + path.string() );
        }

        std::string result;
        result.reserve( static_cast<std::size_t>( size.QuadPart ) );
        char buffer[4096];
        DWORD bytesRead = 0;

        while ( ::ReadFile( file.Get(), buffer, sizeof( buffer ), &bytesRead, nullptr ) != 0 && bytesRead > 0 ) {
            result.append( buffer, bytesRead );
        }

        return result;
    }

    void WriteSecureFile( const std::filesystem::path& path, std::string_view data ) {
        std::filesystem::path temporary = path;
        temporary += L".tmp";

        const HANDLE handle =
            ::CreateFileW( temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );

        if ( handle == INVALID_HANDLE_VALUE ) {
            throw SystemError( "Failed to create temporary file", temporary, ::GetLastError() );
        }

        try {
            {
                FileHandle file( handle );
                std::size_t offset = 0;

                while ( offset < data.size() ) {
                    DWORD written = 0;
                    const auto toWrite = static_cast<DWORD>( data.size() - offset );

                    if ( ::WriteFile( file.Get(), data.data() + offset, toWrite, &written, nullptr ) == 0 ) {
                        throw SystemError( "Failed to write temporary file", temporary, ::GetLastError() );
                    }

                    if ( written == 0 ) {
                        throw std::runtime_error( "Failed to make progress writing " + temporary.string() );
                    }

                    offset += written;
                }

                if ( ::FlushFileBuffers( file.Get() ) == 0 ) {
                    throw SystemError( "Failed to synchronize temporary file", temporary, ::GetLastError() );
                }
            } // close the handle before renaming over the destination

            if ( ::MoveFileExW( temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) == 0 ) {
                throw SystemError( "Failed to install secure file", path, ::GetLastError() );
            }
        } catch ( ... ) {
            ::DeleteFileW( temporary.c_str() );
            throw;
        }
    }

    void SecureDirectory( const std::filesystem::path& ) {
        /*
         * No-op: relies on the per-user profile directory's default ACL
         * rather than an explicit DACL. See CONTRIBUTING.md.
         */
    }

} // namespace ts::client::platform
