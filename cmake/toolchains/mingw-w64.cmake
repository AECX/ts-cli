# CMake toolchain file for cross-compiling ts-cli to 64-bit Windows using
# MinGW-w64, for local build+test on Linux (test binaries run under Wine via
# CMAKE_CROSSCOMPILING_EMULATOR, so `ctest` works unmodified).
#
# Usage:
#   cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
#   cmake --build build-windows
#   ctest --test-dir build-windows --output-on-failure
#
# Requires (Arch package names):
#   mingw-w64-gcc mingw-w64-binutils mingw-w64-crt mingw-w64-headers mingw-w64-winpthreads  (official 'extra' repo)
#   mingw-w64-libsodium mingw-w64-openssl-3 mingw-w64-opus                                  (AUR)
# and `wine` to run the cross-compiled binaries. mingw-w64-opus is only needed for voice
# (audio/src/win32/wasapi_backend.cpp); configure with -DTS_AUDIO_STUB_DEPS=ON to skip it.

set( CMAKE_SYSTEM_NAME Windows )
set( CMAKE_SYSTEM_PROCESSOR x86_64 )

set( TS_MINGW_TRIPLE x86_64-w64-mingw32 )
set( TS_MINGW_SYSROOT /usr/${TS_MINGW_TRIPLE} )

set( CMAKE_C_COMPILER ${TS_MINGW_TRIPLE}-gcc )
set( CMAKE_CXX_COMPILER ${TS_MINGW_TRIPLE}-g++ )
set( CMAKE_RC_COMPILER ${TS_MINGW_TRIPLE}-windres )
set( CMAKE_AR ${TS_MINGW_TRIPLE}-ar CACHE FILEPATH "" )
set( CMAKE_RANLIB ${TS_MINGW_TRIPLE}-ranlib CACHE FILEPATH "" )

set( CMAKE_FIND_ROOT_PATH ${TS_MINGW_SYSROOT} )
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )

# find_library() (including pkg_check_modules(IMPORTED_TARGET ...), which
# resolves each -lfoo to a real file this way) defaults to preferring the
# dynamic import library (.dll.a) over the static archive (.a) on this
# platform. The whole build links statically (see CMAKE_EXE_LINKER_FLAGS_INIT
# below), so flip that preference or the resulting .exe silently ends up
# depending on DLLs (e.g. libsodium-26.dll) at runtime instead. Setting this
# here doesn't stick: CMake's own Windows-GNU platform module resets it while
# processing project(), after the toolchain file's initial pass runs. It's
# set again, for real, in the root CMakeLists.txt right after project().
set( CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dll.a" )

# pkg_check_modules() still invokes the host pkg-config binary; point it at
# the mingw sysroot's .pc files instead of the host's so it reports the
# cross-built libraries (and never accidentally matches host ones).
set( ENV{PKG_CONFIG_LIBDIR} ${TS_MINGW_SYSROOT}/lib/pkgconfig )
set( ENV{PKG_CONFIG_SYSROOT_DIR} ${TS_MINGW_SYSROOT} )

# The AUR mingw-w64-openssl-3 package namespaces its headers/libs under an
# "openssl-3" subdirectory (so it can coexist with an openssl-1.1 package),
# but its own .pc files still point at the un-suffixed paths and are picked
# up by neither pkg-config (wrong PKG_CONFIG_LIBDIR above) nor FindOpenSSL's
# default search. Point CMake straight at the real locations. On the MINGW
# branch, FindOpenSSL.cmake unconditionally re-runs find_library() into
# LIB_EAY/SSL_EAY (not OPENSSL_CRYPTO_LIBRARY/OPENSSL_SSL_LIBRARY directly),
# so pre-seed those instead to short-circuit that search. Prefer the static
# archives since the whole build links statically (see below).
set( OPENSSL_ROOT_DIR ${TS_MINGW_SYSROOT} CACHE PATH "" )
set( OPENSSL_INCLUDE_DIR ${TS_MINGW_SYSROOT}/include/openssl-3 CACHE PATH "" )
set( LIB_EAY ${TS_MINGW_SYSROOT}/lib/openssl-3/libcrypto.a CACHE FILEPATH "" )
set( SSL_EAY ${TS_MINGW_SYSROOT}/lib/openssl-3/libssl.a CACHE FILEPATH "" )

# Statically link the compiler runtime/thread library so the resulting .exe
# runs directly under Wine (or a bare Windows box) without hunting for
# libgcc/libstdc++/libwinpthread DLLs on PATH.
set( CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++" )

find_program( TS_WINE_EXECUTABLE NAMES wine wine64 )

if( TS_WINE_EXECUTABLE )
    set( CMAKE_CROSSCOMPILING_EMULATOR ${TS_WINE_EXECUTABLE} )
else()
    message( WARNING "wine was not found; cross-compiled test binaries will not be runnable via ctest" )
endif()
