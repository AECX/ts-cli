#ifndef TS_PROTOCOL_PACKET_INIT_CODEC_HPP
#define TS_PROTOCOL_PACKET_INIT_CODEC_HPP

#include <array>
#include <cstddef>
#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>
#include <protocol/packet/init_header.hpp>

namespace ts::protocol {

    class InitCodec {
      public:
        [[nodiscard]] static const std::array<std::byte, 8>& Magic();

        static void WriteClientHeader( BinaryWriter& writer, const ClientInitHeader& header );

        [[nodiscard]] static ServerInitHeader ReadServerHeader( BinaryReader& reader );
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_PACKET_INIT_CODEC_HPP
