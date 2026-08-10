#ifndef TS_PROTOCOL_HANDSHAKE_MESSAGE_HPP
#define TS_PROTOCOL_HANDSHAKE_MESSAGE_HPP

#include <cstdint>
#include <protocol/binary_reader.hpp>
#include <protocol/binary_writer.hpp>
#include <protocol/handshake/constants.hpp>
#include <protocol/packet/init_codec.hpp>
#include <protocol/packet/init_header.hpp>
#include <protocol/packet/packet.hpp>
#include <stdexcept>
#include <utility>

namespace ts::protocol {

    template<std::uint8_t Command>
    class ClientHandshakeMessage {
      protected:
        explicit ClientHandshakeMessage( std::uint32_t clientVersion ): m_ClientVersion( clientVersion ) {
        }

        [[nodiscard]] BinaryWriter CreateWriter() const {
            BinaryWriter writer;

            InitCodec::WriteClientHeader( writer,
                                          ClientInitHeader { .packetId = handshake::PacketId,

                                                             .clientId = handshake::InitialClientId,

                                                             .flags = handshake::Flags,

                                                             .version = m_ClientVersion,

                                                             .command = Command } );

            return writer;
        }

        [[nodiscard]] Packet CreatePacket( BinaryWriter writer ) const {
            return Packet( writer.Take() );
        }

      private:
        std::uint32_t m_ClientVersion = 0;
    };

    template<std::uint8_t Command>
    class ServerHandshakeMessage {
      protected:
        [[nodiscard]] static BinaryReader CreateReader( const Packet& packet ) {
            BinaryReader reader( packet.Data() );

            const ServerInitHeader header = InitCodec::ReadServerHeader( reader );

            if ( header.packetId != handshake::PacketId ) {
                throw std::runtime_error( "Invalid handshake packet ID" );
            }

            if ( header.flags != handshake::Flags ) {
                throw std::runtime_error( "Invalid handshake flags" );
            }

            if ( header.command != Command ) {
                throw std::runtime_error( "Unexpected handshake command" );
            }

            return reader;
        }
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_MESSAGE_HPP
