#ifndef TS_PROTOCOL_PACKET_CODEC_HPP
#define TS_PROTOCOL_PACKET_CODEC_HPP

#include "client_header.hpp"
#include "server_header.hpp"

#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>

namespace ts::protocol {

    class PacketCodec {
      public:
        [[nodiscard]] static ServerPacketHeader ReadServerHeader( BinaryReader& reader );

        static void WriteClientMeta( BinaryWriter& writer, const ClientPacketHeader& header );

        static void WriteClientHeader( BinaryWriter& writer, const ClientPacketHeader& header );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_CODEC_HPP
