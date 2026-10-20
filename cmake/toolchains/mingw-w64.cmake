# CMake toolchain file for cross-compiling ts-cli to 64-bit Windows using
# MinGW-w64, for local build+test on Linux (test binaries run under Wine via
# CMAKE_CROSSCOMPILING_EMULATOR, so `ctest` works unmodified).
#
# Usage (`make windows-cross` does all of this):
#   python3 -I cmake/msys2/fetch_packages.py build/deps/msys2-ucrt64 openssl libsodium opus rnnoise
#   cmake -S . -B build/windows-cross -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
#   cmake --build build/windows-cross
#   ctest --test-dir build/windows-cross --output-on-failure
#
# Requires (Arch package names, official repositories):
#   mingw-w64-gcc mingw-w64-binutils mingw-w64-crt mingw-w64-headers mingw-w64-winpthreads
# and `wine` to run the cross-compiled binaries.
#
# Libraries (OpenSSL, libsodium, Opus and RNNoise) are not
# taken from the host. They come prebuilt from MSYS2's UCRT64 repository,
# which uses the same UCRT C runtime and GCC as mingw-w64-gcc, fetched into
# one sysroot by cmake/msys2/fetch_packages.py. Everything links
# dynamically, the way Windows applications are normally shipped: each
# executable is bundled with the DLLs it imports (see cmake/bundle_windows.cmake).

set( CMAKE_SYSTEM_NAME Windows )
set( CMAKE_SYSTEM_PROCESSOR x86_64 )

set( TS_MINGW_TRIPLE x86_64-w64-mingw32 )
set( TS_MINGW_SYSROOT /usr/${TS_MINGW_TRIPLE} )

set( CMAKE_C_COMPILER ${TS_MINGW_TRIPLE}-gcc )
set( CMAKE_CXX_COMPILER ${TS_MINGW_TRIPLE}-g++ )
set( CMAKE_RC_COMPILER ${TS_MINGW_TRIPLE}-windres )
set( CMAKE_AR ${TS_MINGW_TRIPLE}-ar CACHE FILEPATH "" )
set( CMAKE_RANLIB ${TS_MINGW_TRIPLE}-ranlib CACHE FILEPATH "" )

# The fetched MSYS2 prefix (bin/, include/, lib/, share/). MSYS2's .pc files
# say prefix=/ucrt64, so its parent directory is the pkg-config sysroot.
if( NOT TS_WINDOWS_RUNTIME_PREFIX )
    get_filename_component( TS_WINDOWS_RUNTIME_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../build/deps/msys2-ucrt64/ucrt64" ABSOLUTE )
endif()
set( TS_WINDOWS_RUNTIME_PREFIX "${TS_WINDOWS_RUNTIME_PREFIX}" CACHE PATH "MSYS2 UCRT64 prefix providing the Windows libraries" )
get_filename_component( TS_WINDOWS_RUNTIME_SYSROOT "${TS_WINDOWS_RUNTIME_PREFIX}" DIRECTORY )

if( NOT EXISTS "${TS_WINDOWS_RUNTIME_PREFIX}/lib/pkgconfig" )
    message( FATAL_ERROR
        "No Windows libraries at ${TS_WINDOWS_RUNTIME_PREFIX}; fetch them with "
        "cmake/msys2/fetch_packages.py (`make windows-cross` does this, see BUILDING.md)" )
endif()

# try_compile() projects re-read this file; hand them the same prefix.
list( APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES TS_WINDOWS_RUNTIME_PREFIX )

# Two roots: the MSYS2 libraries, and the toolchain's own sysroot for the
# Win32 import libraries (gdi32, ws2_32, ...) that pkg-config files name.
set( CMAKE_FIND_ROOT_PATH "${TS_WINDOWS_RUNTIME_SYSROOT}" ${TS_MINGW_SYSROOT} )
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )

# pkg_check_modules() still invokes the host pkg-config binary; point it at
# the MSYS2 .pc files only, so it never matches host libraries.
set( ENV{PKG_CONFIG_LIBDIR} "${TS_WINDOWS_RUNTIME_PREFIX}/lib/pkgconfig" )
set( ENV{PKG_CONFIG_SYSROOT_DIR} "${TS_WINDOWS_RUNTIME_SYSROOT}" )

set( OPENSSL_ROOT_DIR "${TS_WINDOWS_RUNTIME_PREFIX}" CACHE PATH "" )

# Test binaries need the MSYS2 DLLs; Wine searches WINEPATH like PATH.
find_program( TS_WINE_EXECUTABLE NAMES wine wine64 )

if( TS_WINE_EXECUTABLE )
    set( CMAKE_CROSSCOMPILING_EMULATOR env "WINEPATH=Z:${TS_WINDOWS_RUNTIME_PREFIX}/bin" ${TS_WINE_EXECUTABLE} )
else()
    message( WARNING "wine was not found; cross-compiled test binaries will not be runnable via ctest" )
endif()
