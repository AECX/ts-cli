#include "win32_winsock.hpp"

#include <stdexcept>
#include <string>
#include <winsock2.h>

namespace ts::net {

    namespace {

        class WinsockGuard {
          public:
            WinsockGuard() {
                WSADATA data {};
                const int status = ::WSAStartup( MAKEWORD( 2, 2 ), &data );

                if ( status != 0 ) {
                    throw std::runtime_error( "Failed to initialize Winsock: WSA error " + std::to_string( status ) );
                }
            }

            ~WinsockGuard() {
                ::WSACleanup();
            }

            WinsockGuard( const WinsockGuard& ) = delete;
            WinsockGuard& operator=( const WinsockGuard& ) = delete;
        };

    } // namespace

    void EnsureWinsockInitialized() {
        static WinsockGuard guard;
        (void)guard;
    }

} // namespace ts::net
