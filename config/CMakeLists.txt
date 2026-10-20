add_library(ts-config STATIC
    src/audio_settings.cpp
    src/config.cpp
    src/identity_store.cpp
    src/paths.cpp
    src/setup.cpp
    src/user_config.cpp
)

target_include_directories(ts-config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-config
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-config
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-config
    PUBLIC
        ts-protocol
        ts-audio
)

# Everything above this point is target-agnostic: it only sees the
# ts::config::platform::paths/secure_file interfaces. This is the only place
# that picks a concrete implementation for the build target.
if(WIN32)
    target_sources(ts-config
        PRIVATE
            src/win32/win32_paths.cpp
            src/win32/win32_secure_file.cpp
    )
else()
    target_sources(ts-config
        PRIVATE
            src/posix/posix_paths.cpp
            src/posix/posix_secure_file.cpp
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
