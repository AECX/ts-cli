#include <algorithm>
#include <array>
#include <cstddef>
#include <protocol/packet/init_codec.hpp>
#include <stdexcept>

namespace ts::protocol {

    namespace {

        constexpr std::array<std::byte, 8> InitMagic = { std::byte { 0x54 },
                                                         std::byte { 0x53 },
                                                         std::byte { 0x33 },
                                                         std::byte { 0x49 },
                                                         std::byte { 0x4e },
                                                         std::byte { 0x49 },
                                                         std::byte { 0x54 },
                                                         std::byte { 0x31 } };

    } // namespace

    const std::array<std::byte, 8>& InitCodec::Magic() {
        return InitMagic;
    }

    void InitCodec::WriteClientHeader( BinaryWriter& writer, const ClientInitHeader& header ) {
        writer.WriteBytes( InitMagic );
        writer.WriteU16( header.packetId );
        writer.WriteU16( header.clientId );
        writer.WriteU8( header.flags );
        writer.WriteU32( header.version );
        writer.WriteU8( header.command );
    }

    ServerInitHeader InitCodec::ReadServerHeader( BinaryReader& reader ) {
        const auto magic = reader.ReadBytes( InitMagic.size() );

        if ( !std::equal( InitMagic.begin(), InitMagic.end(), magic.begin() ) ) {
            throw std::runtime_error( "Invalid Init response magic" );
        }

        return ServerInitHeader { .packetId = reader.ReadU16(), .flags = reader.ReadU8(), .command = reader.ReadU8() };
    }

} // namespace ts::protocol
