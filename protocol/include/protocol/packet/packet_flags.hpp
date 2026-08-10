#ifndef TS_PROTOCOL_PACKET_PACKET_FLAGS_HPP
#define TS_PROTOCOL_PACKET_PACKET_FLAGS_HPP

#include <cstdint>

namespace ts::protocol {

    enum class PacketFlags : std::uint8_t {
        None = 0x00,
        Fragmented = 0x10,
        NewProtocol = 0x20,
        Compressed = 0x40,
        Unencrypted = 0x80
    };

    [[nodiscard]] constexpr PacketFlags operator|( PacketFlags left, PacketFlags right ) {
        return static_cast<PacketFlags>( static_cast<std::uint8_t>( left ) | static_cast<std::uint8_t>( right ) );
    }

    [[nodiscard]] constexpr bool HasFlag( PacketFlags value, PacketFlags flag ) {
        return ( static_cast<std::uint8_t>( value ) & static_cast<std::uint8_t>( flag ) ) != 0;
    }

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_PACKET_FLAGS_HPP
