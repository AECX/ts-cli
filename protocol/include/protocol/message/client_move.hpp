#ifndef TS_PROTOCOL_MESSAGE_CLIENT_MOVE_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_MOVE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ts::protocol {

    class ClientMove {
      public:
        ClientMove( std::uint16_t clientId, std::uint64_t channelId );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        std::uint16_t m_ClientId = 0;
        std::uint64_t m_ChannelId = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_MOVE_HPP
