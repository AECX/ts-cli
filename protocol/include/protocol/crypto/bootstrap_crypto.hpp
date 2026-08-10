#ifndef TS_PROTOCOL_CRYPTO_BOOTSTRAP_CRYPTO_HPP
#define TS_PROTOCOL_CRYPTO_BOOTSTRAP_CRYPTO_HPP

#include "aes_eax.hpp"

#include <array>
#include <cstddef>
#include <protocol/packet/server_packet.hpp>
#include <span>
#include <vector>

namespace ts::protocol {

    struct BootstrapEncryptedData {
        std::array<std::byte, 8> mac {};
        std::vector<std::byte> data;
    };

    class BootstrapCrypto {
      public:
        BootstrapCrypto();

        [[nodiscard]] std::vector<std::byte> Decrypt( const ServerPacket& packet ) const;

        [[nodiscard]] BootstrapEncryptedData Encrypt( std::span<const std::byte> meta, std::span<const std::byte> data ) const;

      private:
        AesEax m_AesEax;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_BOOTSTRAP_CRYPTO_HPP
