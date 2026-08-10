#ifndef TS_PROTOCOL_MESSAGE_CLIENT_POKE_HPP
#define TS_PROTOCOL_MESSAGE_CLIENT_POKE_HPP

#include <cstdint>
#include <protocol/command/command.hpp>
#include <string>

namespace ts::protocol {

    struct ClientPokeEntry {
        std::uint16_t invokerId = 0;
        std::string invokerName;
        std::string invokerUniqueId;
        std::string message;
    };

    class ClientPoke {
      public:
        [[nodiscard]] static ClientPoke Parse( const Command& command );
        [[nodiscard]] const ClientPokeEntry& Entry() const;

      private:
        ClientPokeEntry m_Entry;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_CLIENT_POKE_HPP
