#include <algorithm>
#include <protocol/binary_reader.hpp>
#include <protocol/packet/codec.hpp>
#include <protocol/packet/server_packet.hpp>
#include <stdexcept>

namespace ts::protocol {

    ServerPacket ServerPacket::Parse( const Packet& packet ) {
        if ( packet.Size() < 11 ) {
            throw std::runtime_error( "Server packet is too small" );
        }

        BinaryReader reader( packet.Data() );

        ServerPacket result;

        result.m_Header = PacketCodec::ReadServerHeader( reader );

        const auto rawData = packet.Data();

        std::copy( rawData.begin() + 8, rawData.begin() + 11, result.m_Meta.begin() );

        const auto data = reader.ReadBytes( reader.Remaining() );

        result.m_Data.assign( data.begin(), data.end() );

        return result;
    }

    const ServerPacketHeader& ServerPacket::Header() const {
        return m_Header;
    }

    std::span<const std::byte> ServerPacket::Meta() const {
        return m_Meta;
    }

    std::span<const std::byte> ServerPacket::Data() const {
        return m_Data;
    }

} // namespace ts::protocol
