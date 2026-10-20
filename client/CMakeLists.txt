# The network runtime shared by every front end. It processes the terminal
# client's name-based actions too, so it carries their target resolution and
# tree formatting, but no command parsing or terminal output.
add_library(ts-client-runtime STATIC
    src/cli/channel_tree_view.cpp
    src/cli/target_resolver.cpp
    src/runtime/action_processor.cpp
    src/runtime/action_queue.cpp
    src/runtime/event_queue.cpp
    src/runtime/network_runtime.cpp
    src/runtime/server_snapshot.cpp
    src/runtime/talker_activity.cpp
)

target_include_directories(ts-client-runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-client-runtime
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-client-runtime
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-client-runtime
    PUBLIC
        ts-config
        ts-protocol
        ts-audio
        ts-log
)

# The terminal client: slash commands, terminal presentation and console I/O.
add_library(ts-client-support STATIC
    src/cli/command_handler.cpp
    src/cli/command_parser.cpp
    src/cli/presentation.cpp
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
        ts-client-runtime
)

# Everything above this point is target-agnostic: it only sees the
# ts::client::platform::console_io/terminal_control
# interfaces. This is the only place that picks a concrete implementation
# for the build target.
if(WIN32)
    target_sources(ts-client-support
        PRIVATE
            src/platform/win32/win32_console_io.cpp
            src/platform/win32/win32_terminal_control.cpp
    )
else()
    target_sources(ts-client-support
        PRIVATE
            src/platform/posix/posix_console_io.cpp
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

if(WIN32)
    ts_bundle_windows(ts-cli)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
