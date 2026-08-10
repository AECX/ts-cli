#include <algorithm>
#include <client/platform/terminal_control.hpp>
#include <iostream>
#include <ostream>
#include <windows.h>

namespace ts::client::platform {

    namespace {

        HANDLE OutputHandle() {
            return ::GetStdHandle( STD_OUTPUT_HANDLE );
        }

        void ClearRow( HANDLE handle, const CONSOLE_SCREEN_BUFFER_INFO& info, SHORT row ) {
            const COORD start { .X = 0, .Y = row };
            DWORD written = 0;

            ::FillConsoleOutputCharacterA( handle, ' ', static_cast<DWORD>( info.dwSize.X ), start, &written );
            ::FillConsoleOutputAttribute( handle, info.wAttributes, static_cast<DWORD>( info.dwSize.X ), start, &written );
            ::SetConsoleCursorPosition( handle, start );
        }

    } // namespace

    void ClearCurrentLine( std::ostream& stream ) {
        stream.flush();

        const HANDLE handle = OutputHandle();
        CONSOLE_SCREEN_BUFFER_INFO info {};

        if ( ::GetConsoleScreenBufferInfo( handle, &info ) == 0 ) {
            return;
        }

        ClearRow( handle, info, info.dwCursorPosition.Y );
    }

    void ClearPreviousLine( std::ostream& stream ) {
        stream.flush();

        const HANDLE handle = OutputHandle();
        CONSOLE_SCREEN_BUFFER_INFO info {};

        if ( ::GetConsoleScreenBufferInfo( handle, &info ) == 0 ) {
            return;
        }

        const SHORT row = std::max<SHORT>( 0, static_cast<SHORT>( info.dwCursorPosition.Y - 1 ) );

        ClearRow( handle, info, row );
    }

    void ClearScreen( std::ostream& stream ) {
        stream.flush();

        const HANDLE handle = OutputHandle();
        CONSOLE_SCREEN_BUFFER_INFO info {};

        if ( ::GetConsoleScreenBufferInfo( handle, &info ) == 0 ) {
            return;
        }

        const COORD origin { .X = 0, .Y = 0 };
        const DWORD cellCount = static_cast<DWORD>( info.dwSize.X ) * static_cast<DWORD>( info.dwSize.Y );
        DWORD written = 0;

        ::FillConsoleOutputCharacterA( handle, ' ', cellCount, origin, &written );
        ::FillConsoleOutputAttribute( handle, info.wAttributes, cellCount, origin, &written );
    }

} // namespace ts::client::platform
