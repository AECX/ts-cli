cmake_minimum_required(VERSION 3.20)

project(ts-cli
    LANGUAGES CXX
)

if(MINGW)
    # The whole build links statically for MinGW targets (see
    # cmake/toolchains/mingw-w64.cmake), so prefer static archives (.a)
    # over dynamic import libraries (.dll.a) wherever find_library() runs
    # (including pkg_check_modules(IMPORTED_TARGET ...), which resolves
    # each -lfoo this way) -- otherwise the .exe silently ends up depending
    # on DLLs (e.g. libsodium-*.dll) at runtime instead. Must be set here,
    # after project(): CMake's Windows-GNU platform module resets this
    # while processing project(), clobbering any value a toolchain file set.
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dll.a")
endif()

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

add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(protocol)
add_subdirectory(audio)
add_subdirectory(client)
