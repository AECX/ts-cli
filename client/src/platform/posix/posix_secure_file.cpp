#include <cerrno>
#include <client/platform/secure_file.hpp>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace ts::client::platform {

    namespace {

        class FileDescriptor {
          public:
            explicit FileDescriptor( int descriptor ): m_Descriptor( descriptor ) {
            }

            ~FileDescriptor() {
                if ( m_Descriptor >= 0 ) {
                    ::close( m_Descriptor );
                }
            }

            FileDescriptor( const FileDescriptor& ) = delete;
            FileDescriptor& operator=( const FileDescriptor& ) = delete;

            [[nodiscard]] int Get() const {
                return m_Descriptor;
            }

          private:
            int m_Descriptor = -1;
        };

        [[nodiscard]] std::runtime_error SystemError( std::string_view action, const std::filesystem::path& path, int error ) {
            return std::runtime_error( std::string( action ) + ": " + path.string() + ": " + std::strerror( error ) );
        }

    } // namespace

    std::optional<std::string> ReadSecureFile( const std::filesystem::path& path, std::uint64_t maxSize ) {
        const int descriptor = ::open( path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW );

        if ( descriptor < 0 ) {
            if ( errno == ENOENT ) {
                return std::nullopt;
            }

            throw SystemError( "Failed to open secure file", path, errno );
        }

        FileDescriptor file( descriptor );
        struct stat information {};

        if ( ::fstat( file.Get(), &information ) != 0 ) {
            throw SystemError( "Failed to inspect secure file", path, errno );
        }

        if ( !S_ISREG( information.st_mode ) ) {
            throw std::runtime_error( "Not a regular file: " + path.string() );
        }

        if ( information.st_size < 0 || static_cast<std::uint64_t>( information.st_size ) > maxSize ) {
            throw std::runtime_error( "Secure file is too large: " + path.string() );
        }

        if ( ::fchmod( file.Get(), S_IRUSR | S_IWUSR ) != 0 ) {
            throw SystemError( "Failed to secure file", path, errno );
        }

        std::string result;
        result.reserve( static_cast<std::size_t>( information.st_size ) );
        char buffer[4096];

        while ( true ) {
            const ssize_t count = ::read( file.Get(), buffer, sizeof( buffer ) );

            if ( count == 0 ) {
                break;
            }

            if ( count < 0 ) {
                if ( errno == EINTR ) {
                    continue;
                }

                throw SystemError( "Failed to read secure file", path, errno );
            }

            result.append( buffer, static_cast<std::size_t>( count ) );
        }

        return result;
    }

    void WriteSecureFile( const std::filesystem::path& path, std::string_view data ) {
        std::string temporary = path.string() + ".tmp.XXXXXX";
        const int descriptor = ::mkstemp( temporary.data() );

        if ( descriptor < 0 ) {
            throw SystemError( "Failed to create temporary file", path, errno );
        }

        FileDescriptor file( descriptor );

        try {
            if ( ::fcntl( file.Get(), F_SETFD, FD_CLOEXEC ) == -1 ) {
                throw SystemError( "Failed to configure temporary file", temporary, errno );
            }

            if ( ::fchmod( file.Get(), S_IRUSR | S_IWUSR ) != 0 ) {
                throw SystemError( "Failed to secure temporary file", temporary, errno );
            }

            std::size_t offset = 0;

            while ( offset < data.size() ) {
                const ssize_t count = ::write( file.Get(), data.data() + offset, data.size() - offset );

                if ( count < 0 ) {
                    if ( errno == EINTR ) {
                        continue;
                    }

                    throw SystemError( "Failed to write temporary file", temporary, errno );
                }

                if ( count == 0 ) {
                    throw std::runtime_error( "Failed to make progress writing " + temporary );
                }

                offset += static_cast<std::size_t>( count );
            }

            if ( ::fsync( file.Get() ) != 0 ) {
                throw SystemError( "Failed to synchronize temporary file", temporary, errno );
            }

            if ( ::rename( temporary.c_str(), path.c_str() ) != 0 ) {
                throw SystemError( "Failed to install secure file", path, errno );
            }
        } catch ( ... ) {
            ::unlink( temporary.c_str() );
            throw;
        }
    }

    void SecureDirectory( const std::filesystem::path& path ) {
        if ( ::chmod( path.c_str(), S_IRWXU ) != 0 ) {
            throw std::runtime_error( "Failed to secure directory: " + path.string() );
        }
    }

} // namespace ts::client::platform
