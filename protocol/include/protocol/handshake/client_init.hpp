#ifndef TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_HPP
#define TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_HPP

#include <cstddef>
#include <cstdint>
#include <protocol/client_profile.hpp>
#include <vector>

namespace ts::protocol {

    class ClientInit {
      public:
        ClientInit( ClientProfile profile, std::uint64_t keyOffset );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        ClientProfile m_Profile;

        std::uint64_t m_KeyOffset = 0;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_CLIENT_INIT_HPP
