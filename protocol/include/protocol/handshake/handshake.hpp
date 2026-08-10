#ifndef TS_PROTOCOL_HANDSHAKE_HANDSHAKE_HPP
#define TS_PROTOCOL_HANDSHAKE_HANDSHAKE_HPP

#include <cstdint>
#include <protocol/identity.hpp>
#include <protocol/session/bootstrap.hpp>
#include <protocol/transport.hpp>
#include <string>

namespace ts::protocol {

    class Handshake {
      public:
        Handshake( Transport& transport, Identity& identity, std::uint32_t clientVersion, std::string serverIp );

        SessionBootstrap Run();

      private:
        Transport& m_Transport;
        Identity& m_Identity;

        std::uint32_t m_ClientVersion = 0;

        std::string m_ServerIp;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_HANDSHAKE_HPP
