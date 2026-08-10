add_executable(ts-client-tests
    main.cpp
    action_queue_test.cpp
    channel_tree_view_test.cpp
    command_handler_test.cpp
    command_parser_test.cpp
    config_test.cpp
    event_queue_test.cpp
    presentation_test.cpp
    setup_test.cpp
    target_resolver_test.cpp
    user_config_test.cpp
)

target_compile_features(ts-client-tests
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-client-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-client-tests
    PRIVATE
        ts-client-support
)

add_test(
    NAME ts-client
    COMMAND ts-client-tests
)
