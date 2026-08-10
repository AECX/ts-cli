add_library(ts-protocol STATIC
    src/binary_reader.cpp
    src/binary_writer.cpp
    src/connection.cpp
    src/identity.cpp
    src/transport.cpp
    src/session_transport.cpp
    src/connection_statistics.cpp

    src/session/session.cpp

    src/message/channel_list.cpp
    src/message/channel_created.cpp
    src/message/channel_deleted.cpp
    src/message/channel_edited.cpp
    src/message/channel_moved.cpp
    src/message/channel_subscription.cpp
    src/message/client_enter_view.cpp
    src/message/client_left_view.cpp
    src/message/client_moved.cpp
    src/message/text_message.cpp
    src/message/client_disconnect.cpp
    src/message/client_move.cpp
    src/message/client_update.cpp
    src/message/client_updated.cpp
    src/message/command_result.cpp
    src/message/send_text_message.cpp
    src/message/set_connection_info.cpp


    src/reliability/reliable_command_queue.cpp

    src/state/channel_store.cpp
    src/state/client_store.cpp

    src/command/parser.cpp
    src/command/writer.cpp

    src/compression/quick_lz.cpp

    src/crypto/bootstrap_crypto.cpp
    src/crypto/hash_cash.cpp
    src/crypto/p256.cpp
    src/crypto/puzzle_solver.cpp
    src/crypto/session_crypto.cpp
    src/crypto/session_material.cpp
    src/crypto/aes_eax.cpp

    src/encoding/base64.cpp

    src/handshake/bootstrap_transport.cpp
    src/handshake/client_ek.cpp
    src/handshake/client_init.cpp
    src/handshake/client_init_iv.cpp
    src/handshake/get_cookie.cpp
    src/handshake/get_puzzle.cpp
    src/handshake/handshake.cpp
    src/handshake/init_iv_expand_2.cpp
    src/handshake/init_server.cpp
    src/handshake/license.cpp
    src/handshake/puzzle.cpp
    src/handshake/puzzle_answer.cpp
    src/handshake/set_cookie.cpp

    src/packet/sequence_state.cpp
    src/packet/command_fragmenter.cpp
    src/packet/codec.cpp
    src/packet/init_codec.cpp
    src/packet/packet.cpp
    src/packet/server_packet.cpp

    src/voice/voice_codec.cpp
)

target_include_directories(ts-protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-protocol
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-protocol
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

target_link_libraries(ts-protocol
    PUBLIC
        ts-net
        OpenSSL::Crypto

    PRIVATE
        PkgConfig::SODIUM
)

if(WIN32)
    # Static OpenSSL pulls in a handful of Win32 system libraries for
    # CryptoAPI/certificate-store and socket-adjacent symbols.
    target_link_libraries(ts-protocol
        PRIVATE
            ws2_32
            gdi32
            crypt32
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
