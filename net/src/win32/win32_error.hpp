#ifndef TS_NET_WIN32_ERROR_HPP
#define TS_NET_WIN32_ERROR_HPP

#include <string>
#include <winsock2.h>

namespace ts::net {

    /* Formats a WSA/Win32 error code the way std::strerror formats errno. */
    inline std::string FormatWsaError( int code ) {
        char buffer[256] {};

        const DWORD length = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                               nullptr,
                                               static_cast<DWORD>( code ),
                                               0,
                                               buffer,
                                               sizeof( buffer ),
                                               nullptr );

        if ( length == 0 ) {
            return "WSA error " + std::to_string( code );
        }

        std::string message( buffer, length );

        while ( !message.empty() && ( message.back() == '\n' || message.back() == '\r' ) ) {
            message.pop_back();
        }

        return message;
    }

} // namespace ts::net

#endif // TS_NET_WIN32_ERROR_HPP
