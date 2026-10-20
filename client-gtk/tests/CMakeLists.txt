add_executable(ts-client-gtk-tests
    main.cpp
    conversation_log_test.cpp
    event_router_test.cpp
)

target_compile_features(ts-client-gtk-tests
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-client-gtk-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-client-gtk-tests
    PRIVATE
        ts-client-gtk-support
)

add_test(
    NAME ts-client-gtk
    COMMAND ts-client-gtk-tests
)
