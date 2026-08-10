add_library(ts-client-support STATIC
    src/cli/channel_tree_view.cpp
    src/cli/command_handler.cpp
    src/cli/command_parser.cpp
    src/cli/presentation.cpp
    src/cli/target_resolver.cpp
    src/config/config.cpp
    src/config/identity_store.cpp
    src/config/user_config.cpp
    src/config/paths.cpp
    src/config/setup.cpp
    src/runtime/action_processor.cpp
    src/runtime/action_queue.cpp
    src/runtime/event_queue.cpp
    src/runtime/network_runtime.cpp
)

target_include_directories(ts-client-support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-client-support
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-client-support
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-client-support
    PUBLIC
        ts-protocol
        ts-audio
        ts-log
)

# Everything above this point is target-agnostic: it only sees the
# ts::client::platform::console_io/secure_file/paths/terminal_control
# interfaces. This is the only place that picks a concrete implementation
# for the build target.
if(WIN32)
    target_sources(ts-client-support
        PRIVATE
            src/platform/win32/win32_console_io.cpp
            src/platform/win32/win32_secure_file.cpp
            src/platform/win32/win32_paths.cpp
            src/platform/win32/win32_terminal_control.cpp
    )

    # Statically link winpthreads (the mingw-w64-gcc "posix" thread model
    # backing std::thread/condition_variable) so the win32 console backend's
    # reader thread doesn't need a runtime DLL alongside the .exe.
    target_link_libraries(ts-client-support
        PRIVATE
            -static
            winpthread
    )
else()
    target_sources(ts-client-support
        PRIVATE
            src/platform/posix/posix_console_io.cpp
            src/platform/posix/posix_secure_file.cpp
            src/platform/posix/posix_paths.cpp
            src/platform/posix/posix_terminal_control.cpp
    )
endif()

add_executable(ts-cli
    src/main.cpp
)

target_compile_features(ts-cli
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-cli
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-cli
    PRIVATE
        ts-client-support
        ts-protocol
        ts-log
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
