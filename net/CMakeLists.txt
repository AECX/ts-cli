add_library(ts-net STATIC
    src/endpoint.cpp
    src/endpoint_resolution.cpp
    src/udp_socket.cpp
)

target_include_directories(ts-net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ts-net
    PUBLIC
        cxx_std_20
)

target_compile_options(ts-net
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
)

# Everything above this point is target-agnostic: it only sees the opaque
# net::Address and the SocketBackend interface. The block below is the only
# place that picks a concrete backend implementation for the build target.
if(WIN32)
    target_sources(ts-net
        PRIVATE
            src/win32/win32_address.cpp
            src/win32/win32_socket_backend.cpp
            src/win32/win32_srv_record.cpp
            src/win32/win32_winsock.cpp
    )

    # Statically link winpthreads (the mingw-w64-gcc "posix" thread model
    # backing std::thread/std::promise) so the bounded SRV lookup in
    # endpoint_resolution.cpp doesn't need a runtime DLL alongside the .exe.
    target_link_libraries(ts-net
        PRIVATE
            -static
            winpthread
            ws2_32
            dnsapi
    )
else()
    target_sources(ts-net
        PRIVATE
            src/posix/posix_address.cpp
            src/posix/posix_socket_backend.cpp
            src/posix/posix_srv_record.cpp
    )

    target_link_libraries(ts-net
        PRIVATE
            resolv
    )
endif()
