add_library(ts-log STATIC
    src/logger.cpp
    src/ostream_sink.cpp
)

target_include_directories(ts-log
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Everything above this point is target-agnostic: it only sees the
# log::TerminalColor/TerminalStream interface. This is the only place
# that picks a concrete implementation for the build target.
if(WIN32)
    target_sources(ts-log
        PRIVATE
            src/win32/win32_terminal_color.cpp
    )
else()
    target_sources(ts-log
        PRIVATE
            src/posix/posix_terminal_color.cpp
    )
endif()
