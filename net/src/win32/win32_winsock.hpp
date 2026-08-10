#ifndef TS_NET_WIN32_WINSOCK_HPP
#define TS_NET_WIN32_WINSOCK_HPP

namespace ts::net {

    /*
     * Ensures WSAStartup has been called exactly once for this process,
     * regardless of which win32 backend file needs Winsock first (address
     * resolution can run before any socket is created). Private to the
     * win32 backend; not part of net's public API.
     */
    void EnsureWinsockInitialized();

} // namespace ts::net

#endif // TS_NET_WIN32_WINSOCK_HPP
