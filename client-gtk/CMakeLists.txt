# Toolkit-independent GUI logic (conversation routing and history). Kept
# separate from the GTK widgets so it is unit-testable without a display.
add_library(ts-client-gtk-support STATIC
    src/conversation_log.cpp
    src/event_router.cpp
)

target_include_directories(ts-client-gtk-support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-client-gtk-support
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-client-gtk-support
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-client-gtk-support
    PUBLIC
        ts-client-support
)

pkg_check_modules(
    GTKMM
    REQUIRED
    IMPORTED_TARGET
    gtkmm-4.0
)

add_executable(ts-gtk
    src/audio_settings_window.cpp
    src/channel_tree.cpp
    src/display_text.cpp
    src/feed_view.cpp
    src/main.cpp
    src/main_window.cpp
    src/user_audio_popover.cpp
)

target_compile_features(ts-gtk
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-gtk
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-gtk
    PRIVATE
        ts-client-gtk-support
        ts-client-support
        ts-protocol
        ts-log
        PkgConfig::GTKMM
)

# Everything above this point is target-agnostic: it only sees the
# ts::client::gtk::platform interfaces. This is the only place that picks a
# concrete implementation for the build target.
if(WIN32)
    target_sources(ts-gtk
        PRIVATE
            src/platform/win32/win32_shutdown_signal.cpp
    )
else()
    target_sources(ts-gtk
        PRIVATE
            src/platform/posix/posix_shutdown_signal.cpp
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(WIN32)
    # A window, not a console program.
    set_target_properties(ts-gtk PROPERTIES WIN32_EXECUTABLE ON)
    ts_bundle_windows(ts-gtk GTK_DATA)
endif()
