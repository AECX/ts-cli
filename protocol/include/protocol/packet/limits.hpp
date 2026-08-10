#ifndef TS_PROTOCOL_PACKET_LIMITS_HPP
#define TS_PROTOCOL_PACKET_LIMITS_HPP

#include <cstddef>

namespace ts::protocol::packet_limits {

    inline constexpr std::size_t MaxDatagramSize = 500;
    inline constexpr std::size_t ClientHeaderSize = 13;
    inline constexpr std::size_t ServerHeaderSize = 11;

    inline constexpr std::size_t MaxClientPayload = MaxDatagramSize - ClientHeaderSize;
    inline constexpr std::size_t MaxServerPayload = MaxDatagramSize - ServerHeaderSize;

    inline constexpr std::size_t MaxCommandSize = 64 * 1024;

    // TeamSpeak connection statistics count the UDP + IPv4 framing as well
    // as the TeamSpeak datagram itself (matching mature client behavior).
    inline constexpr std::size_t UdpHeaderSize = 8;
    inline constexpr std::size_t Ipv4HeaderSize = 20;
    inline constexpr std::size_t Ipv4UdpOverhead = UdpHeaderSize + Ipv4HeaderSize;

    static_assert( MaxClientPayload == 487 );
    static_assert( MaxServerPayload == 489 );

} // namespace ts::protocol::packet_limits

#endif // TS_PROTOCOL_PACKET_LIMITS_HPP
