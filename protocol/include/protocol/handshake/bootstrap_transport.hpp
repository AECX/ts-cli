#ifndef TS_PROTOCOL_HANDSHAKE_BOOTSTRAP_TRANSPORT_HPP
#define TS_PROTOCOL_HANDSHAKE_BOOTSTRAP_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <protocol/crypto/bootstrap_crypto.hpp>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/sequence_state.hpp>
#include <protocol/transport.hpp>
#include <span>
#include <vector>

namespace ts::protocol {

    class ServerPacket;

    class BootstrapTransport {
      public:
        explicit BootstrapTransport( Transport& transport );

        [[nodiscard]] std::vector<std::byte> ReceiveCommand();

        std::uint16_t SendCommand( std::span<const std::byte> data );

        void ReceiveAck( std::uint16_t expectedCommandPacketId );

        [[nodiscard]] PacketSequenceState SequenceState() const;

      private:
        static constexpr std::size_t MaxFragments = 64;

        static constexpr std::size_t MaxCommandSize = 64 * 1024;

        static constexpr std::size_t MaxClientPacketData = 487;

        [[nodiscard]] ServerPacket ReceivePacket();

        void SendAck( const ServerPacket& packet );

        void SendEncrypted( PacketType type, PacketFlags flags, std::span<const std::byte> data );

        Transport& m_Transport;
        BootstrapCrypto m_Crypto;
        PacketSequenceState m_Sequences;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_BOOTSTRAP_TRANSPORT_HPP
