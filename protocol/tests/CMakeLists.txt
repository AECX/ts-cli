add_executable(ts-protocol-tests
    main.cpp

    binary_reader_test.cpp
    bootstrap_crypto_test.cpp
    command_parser_test.cpp
    command_writer_test.cpp
    command_fragmenter_test.cpp
    connection_statistics_test.cpp
    set_connection_info_test.cpp
    packet_sequence_test.cpp
    quick_lz_test.cpp
    session_crypto_test.cpp
    channel_state_test.cpp
    client_state_test.cpp
    identity_test.cpp
    reliable_command_queue_test.cpp
    text_message_test.cpp
    client_disconnect_test.cpp
    client_command_test.cpp
    voice_codec_test.cpp

)

target_compile_features(ts-protocol-tests
    PRIVATE
        cxx_std_20
)

target_compile_options(ts-protocol-tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-protocol-tests
    PRIVATE
        ts-protocol
)

add_test(
    NAME ts-protocol
    COMMAND ts-protocol-tests
)
