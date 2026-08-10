#include <algorithm>
#include <array>
#include <cstddef>
#include <protocol/crypto/bootstrap_crypto.hpp>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    constexpr std::array<std::byte, 16> BootstrapKey = { std::byte { 0x63 },
                                                         std::byte { 0x3a },
                                                         std::byte { 0x5c },
                                                         std::byte { 0x77 },
                                                         std::byte { 0x69 },
                                                         std::byte { 0x6e },
                                                         std::byte { 0x64 },
                                                         std::byte { 0x6f },
                                                         std::byte { 0x77 },
                                                         std::byte { 0x73 },
                                                         std::byte { 0x5c },
                                                         std::byte { 0x73 },
                                                         std::byte { 0x79 },
                                                         std::byte { 0x73 },
                                                         std::byte { 0x74 },
                                                         std::byte { 0x65 } };

    constexpr std::array<std::byte, 16> BootstrapNonce = { std::byte { 0x6d },
                                                           std::byte { 0x5c },
                                                           std::byte { 0x66 },
                                                           std::byte { 0x69 },
                                                           std::byte { 0x72 },
                                                           std::byte { 0x65 },
                                                           std::byte { 0x77 },
                                                           std::byte { 0x61 },
                                                           std::byte { 0x6c },
                                                           std::byte { 0x6c },
                                                           std::byte { 0x33 },
                                                           std::byte { 0x32 },
                                                           std::byte { 0x2e },
                                                           std::byte { 0x63 },
                                                           std::byte { 0x70 },
                                                           std::byte { 0x6c } };

    BootstrapCrypto::BootstrapCrypto(): m_AesEax( BootstrapKey ) {
    }

    std::vector<std::byte> BootstrapCrypto::Decrypt( const ServerPacket& packet ) const {
        auto plaintext = m_AesEax.Decrypt( BootstrapNonce, packet.Meta(), packet.Data(), packet.Header().mac );

        if ( !plaintext ) {
            throw std::runtime_error( "Invalid bootstrap packet MAC" );
        }

        return std::move( *plaintext );
    }

    BootstrapEncryptedData BootstrapCrypto::Encrypt( std::span<const std::byte> meta, std::span<const std::byte> data ) const {
        AesEaxEncryptedData encrypted = m_AesEax.Encrypt( BootstrapNonce, meta, data );

        BootstrapEncryptedData result;

        std::copy_n( encrypted.tag.begin(), result.mac.size(), result.mac.begin() );

        result.data = std::move( encrypted.data );

        return result;
    }

} // namespace ts::protocol
