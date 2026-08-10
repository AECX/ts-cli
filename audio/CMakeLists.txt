option(TS_AUDIO_STUB_DEPS "Build audio with stub backend/codec (tests and dependency-free verification only)" OFF)

add_library(ts-audio STATIC
    src/audio_engine.cpp
    src/capture_filter.cpp
    src/voice_jitter_buffer.cpp
    src/notification_synthesizer.cpp
)

target_include_directories(ts-audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-audio PUBLIC cxx_std_20)

target_compile_options(ts-audio
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

if(TS_AUDIO_STUB_DEPS)
    target_sources(ts-audio PRIVATE
        src/stub_backend.cpp
        src/stub_codec.cpp
    )
else()
    # Opus has no platform dependency: it's wired in unconditionally here,
    # independent of which concrete AudioBackend gets picked below.
    pkg_check_modules(
        OPUS
        REQUIRED
        IMPORTED_TARGET
        opus
    )

    target_sources(ts-audio PRIVATE src/opus_codec.cpp)
    target_link_libraries(ts-audio PRIVATE PkgConfig::OPUS)

    # Everything above this point (within this not-stubbed branch) is
    # target-agnostic. The block below is the only place that picks a
    # concrete AudioBackend implementation for the build target.
    if(WIN32)
        target_sources(ts-audio PRIVATE src/win32/wasapi_backend.cpp)

        # Statically link winpthreads (MinGW's std::thread/std::promise
        # backing the backend's control/capture/render threads) plus the
        # COM/WASAPI import libraries the backend links against directly.
        target_link_libraries(ts-audio
            PRIVATE
                -static
                winpthread
                ole32
                oleaut32
                uuid
                ksuser
                avrt
                mmdevapi
                propsys
        )
    else()
        pkg_check_modules(
            PIPEWIRE
            REQUIRED
            IMPORTED_TARGET
            libpipewire-0.3
        )

        target_sources(ts-audio PRIVATE src/posix/pipewire_backend.cpp)
        target_link_libraries(ts-audio PRIVATE PkgConfig::PIPEWIRE)
    endif()

    option(TS_AUDIO_RNNOISE "Enable RNNoise capture filtering when the library is available" ON)

    if(TS_AUDIO_RNNOISE)
        pkg_check_modules(
            RNNOISE
            QUIET
            IMPORTED_TARGET
            rnnoise
        )

        if(RNNOISE_FOUND)
            target_sources(ts-audio PRIVATE src/rnnoise_filter.cpp)
            target_compile_definitions(ts-audio PRIVATE TS_AUDIO_HAVE_RNNOISE=1)
            target_link_libraries(ts-audio PRIVATE PkgConfig::RNNOISE)
        else()
            message(STATUS "RNNoise not found; capture filter 'rnnoise' will be unavailable")
        endif()
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
