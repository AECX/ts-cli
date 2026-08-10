#ifndef TS_PROTOCOL_CRYPTO_SESSION_CRYPTO_HPP
#define TS_PROTOCOL_CRYPTO_SESSION_CRYPTO_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <protocol/packet/packet_type.hpp>
#include <protocol/packet/server_packet.hpp>
#include <span>
#include <vector>

namespace ts::protocol {

    struct SessionEncryptedData {
        std::array<std::byte, 8> mac {};
        std::vector<std::byte> data;
    };

    class SessionCrypto {
      public:
        SessionCrypto( const std::array<std::byte, 64>& sharedIv, const std::array<std::byte, 8>& sharedMac );

        [[nodiscard]] SessionEncryptedData EncryptClient( PacketType type,
                                                          std::uint16_t packetId,
                                                          std::uint32_t generationId,
                                                          std::span<const std::byte> meta,
                                                          std::span<const std::byte> data ) const;

        [[nodiscard]] std::vector<std::byte> DecryptServer( const ServerPacket& packet, std::uint32_t generationId ) const;

        [[nodiscard]] const std::array<std::byte, 8>& SharedMac() const;

      private:
        struct KeyNonce {
            std::array<std::byte, 16> key {};
            std::array<std::byte, 16> nonce {};
        };

        [[nodiscard]] KeyNonce
            CreateKeyNonce( PacketType type, bool clientToServer, std::uint16_t packetId, std::uint32_t generationId ) const;

        [[nodiscard]] static std::array<std::byte, 32> Sha256( std::span<const std::byte> data );

        std::array<std::byte, 64> m_SharedIv {};
        std::array<std::byte, 8> m_SharedMac {};
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_SESSION_CRYPTO_HPP
