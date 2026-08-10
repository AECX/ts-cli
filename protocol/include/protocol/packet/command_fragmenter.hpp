#ifndef TS_PROTOCOL_PACKET_COMMAND_FRAGMENTER_HPP
#define TS_PROTOCOL_PACKET_COMMAND_FRAGMENTER_HPP

#include <cstddef>
#include <protocol/packet/packet_flags.hpp>
#include <vector>

namespace ts::protocol {

    struct CommandFragment {
        std::size_t offset = 0;
        std::size_t size = 0;
        PacketFlags flags = PacketFlags::None;
    };

    class CommandFragmenter {
      public:
        [[nodiscard]] static std::vector<CommandFragment> Plan( std::size_t commandSize );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_COMMAND_FRAGMENTER_HPP
