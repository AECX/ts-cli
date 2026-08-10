#ifndef TS_PROTOCOL_HANDSHAKE_INIT_SERVER_HPP
#define TS_PROTOCOL_HANDSHAKE_INIT_SERVER_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>

namespace ts::protocol {

    class InitServer {
      public:
        [[nodiscard]] static InitServer Parse( const Command& command );

        [[nodiscard]] std::uint16_t ClientId() const;

        [[nodiscard]] std::string_view ServerName() const;
        [[nodiscard]] std::uint8_t CodecEncryptionMode() const;

      private:
        std::uint16_t m_ClientId = 0;
        std::string m_ServerName;
        std::uint8_t m_CodecEncryptionMode = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_INIT_SERVER_HPP
