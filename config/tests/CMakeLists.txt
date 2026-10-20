add_executable(ts-config-tests
    main.cpp
    audio_settings_test.cpp
    config_test.cpp
    setup_test.cpp
    user_config_test.cpp
)

target_compile_features(ts-config-tests
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-config-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-config-tests
    PRIVATE
        ts-config
)

add_test(
    NAME ts-config
    COMMAND ts-config-tests
)
