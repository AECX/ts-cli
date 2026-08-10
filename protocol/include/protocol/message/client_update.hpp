#ifndef TS_PROTOCOL_MESSAGE_CLIENT_UPDATE_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_UPDATE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace ts::protocol {

    class ClientUpdate {
      public:
        explicit ClientUpdate( std::string nickname );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        std::string m_Nickname;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_UPDATE_HPP
