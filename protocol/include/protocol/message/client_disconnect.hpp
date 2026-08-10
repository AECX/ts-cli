#ifndef TS_PROTOCOL_MESSAGE_CLIENT_DISCONNECT_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_DISCONNECT_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace ts::protocol {

    class ClientDisconnect {
      public:
        explicit ClientDisconnect( std::string reason );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        std::string m_Reason;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_DISCONNECT_HPP
