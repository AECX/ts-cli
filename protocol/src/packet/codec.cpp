#include <algorithm>
#include <cstdint>
#include <protocol/packet/codec.hpp>

namespace ts::protocol {

    ServerPacketHeader PacketCodec::ReadServerHeader( BinaryReader& reader ) {
        ServerPacketHeader result;

        const auto mac = reader.ReadBytes( result.mac.size() );

        std::copy( mac.begin(), mac.end(), result.mac.begin() );

        result.packetId = reader.ReadU16();

        const std::uint8_t typeAndFlags = reader.ReadU8();

        result.type = static_cast<PacketType>( typeAndFlags & 0x0f );

        result.flags = static_cast<PacketFlags>( typeAndFlags & 0xf0 );

        return result;
    }

    void PacketCodec::WriteClientMeta( BinaryWriter& writer, const ClientPacketHeader& header ) {
        writer.WriteU16( header.packetId );

        writer.WriteU16( header.clientId );

        writer.WriteU8( static_cast<std::uint8_t>( header.type ) | static_cast<std::uint8_t>( header.flags ) );
    }

    void PacketCodec::WriteClientHeader( BinaryWriter& writer, const ClientPacketHeader& header ) {
        writer.WriteBytes( header.mac );

        WriteClientMeta( writer, header );
    }

} // namespace ts::protocol
