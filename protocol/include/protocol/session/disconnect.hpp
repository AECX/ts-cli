#ifndef TS_PROTOCOL_SESSION_DISCONNECT_HPP
#define TS_PROTOCOL_SESSION_DISCONNECT_HPP

#include <cstdint>
#include <string>

namespace ts::protocol {

    enum class DisconnectReason {
        /* The caller asked to leave, via Connection::Disconnect or a stop request. */
        LocalRequest,

        /* The server removed us: kick, ban, or server shutdown. */
        ServerClosed,

        /* Nothing was received for SessionTransport::ReceiveTimeout. */
        Timeout,

        /* An exception escaped the session loop; message carries what(). */
        TransportError
    };

    /*
     * A server-initiated removal, as reported by notifyclientleftview for our
     * own client id.
     */
    struct ServerDisconnect {
        /* TeamSpeak reasonid, for example 5 for a kick from the server. */
        std::uint64_t reasonId = 0;

        std::string message;
    };

    struct DisconnectInfo {
        DisconnectReason reason = DisconnectReason::LocalRequest;

        /* Only meaningful when reason is ServerClosed. */
        std::uint64_t serverReasonId = 0;

        /*
         * Human-readable detail. For ServerClosed this is the server's reason
         * message, which is frequently empty; for TransportError it is the
         * exception text. Never assume it is non-empty.
         */
        std::string message;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_DISCONNECT_HPP
