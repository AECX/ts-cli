#include <algorithm>
#include <cstddef>
#include <protocol/packet/command_fragmenter.hpp>
#include <protocol/packet/limits.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <stdexcept>
#include <vector>

namespace ts::protocol {

    std::vector<CommandFragment> CommandFragmenter::Plan( std::size_t commandSize ) {
        if ( commandSize > packet_limits::MaxCommandSize ) {
            throw std::runtime_error( "Session client command is too large" );
        }

        if ( commandSize <= packet_limits::MaxClientPayload ) {
            return { CommandFragment {
                .offset = 0,
                .size = commandSize,
                .flags = PacketFlags::NewProtocol,
            } };
        }

        std::vector<CommandFragment> fragments;
        fragments.reserve( ( commandSize + packet_limits::MaxClientPayload - 1 ) / packet_limits::MaxClientPayload );

        std::size_t offset = 0;
        while ( offset < commandSize ) {
            const std::size_t fragmentSize = std::min( packet_limits::MaxClientPayload, commandSize - offset );
            const bool firstFragment = offset == 0;
            const bool lastFragment = offset + fragmentSize == commandSize;

            PacketFlags flags = PacketFlags::NewProtocol;
            if ( firstFragment || lastFragment ) {
                flags = flags | PacketFlags::Fragmented;
            }

            fragments.push_back( CommandFragment {
                .offset = offset,
                .size = fragmentSize,
                .flags = flags,
            } );

            offset += fragmentSize;
        }

        return fragments;
    }

} // namespace ts::protocol
