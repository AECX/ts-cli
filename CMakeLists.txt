cmake_minimum_required(VERSION 3.20)

project(ts-cli
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    SODIUM
    REQUIRED
    IMPORTED_TARGET
    libsodium
)

include(CTest)

# Windows has no system package manager for these libraries, so each
# executable is shipped as a folder with the DLLs it imports, taken from
# this MSYS2 UCRT64 prefix (the build environment itself for windows-msys;
# set by cmake/toolchains/mingw-w64.cmake for windows-cross). Linux builds
# link the distribution's shared libraries and need nothing of the sort.
if(WIN32)
    set(TS_WINDOWS_RUNTIME_PREFIX "" CACHE PATH "MSYS2 UCRT64 prefix providing the Windows libraries")

    if(NOT TS_WINDOWS_RUNTIME_PREFIX)
        message(FATAL_ERROR
            "Windows builds need -DTS_WINDOWS_RUNTIME_PREFIX=<MSYS2 ucrt64 prefix>; "
            "`make windows-cross` and `make windows-msys` set it (see BUILDING.md)")
    endif()

    include(cmake/bundle_windows_target.cmake)
endif()

add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(protocol)
add_subdirectory(audio)
add_subdirectory(config)
add_subdirectory(client)
