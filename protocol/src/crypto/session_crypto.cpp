#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <openssl/evp.h>
#include <protocol/crypto/aes_eax.hpp>
#include <protocol/crypto/session_crypto.hpp>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ts::protocol {

    SessionCrypto::SessionCrypto( const std::array<std::byte, 64>& sharedIv, const std::array<std::byte, 8>& sharedMac ):
        m_SharedIv( sharedIv ), m_SharedMac( sharedMac ) {
    }

    SessionCrypto::KeyNonce SessionCrypto::CreateKeyNonce( PacketType type,
                                                           bool clientToServer,
                                                           std::uint16_t packetId,
                                                           std::uint32_t generationId ) const {
        std::array<std::byte, 70> input {};

        input[0] = clientToServer ? std::byte { 0x31 } : std::byte { 0x30 };

        input[1] = static_cast<std::byte>( static_cast<std::uint8_t>( type ) );

        input[2] = static_cast<std::byte>( ( generationId >> 24 ) & 0xff );

        input[3] = static_cast<std::byte>( ( generationId >> 16 ) & 0xff );

        input[4] = static_cast<std::byte>( ( generationId >> 8 ) & 0xff );

        input[5] = static_cast<std::byte>( generationId & 0xff );

        std::copy( m_SharedIv.begin(), m_SharedIv.end(), input.begin() + 6 );

        const auto digest = Sha256( input );

        KeyNonce result;

        std::copy_n( digest.begin(), result.key.size(), result.key.begin() );

        std::copy_n( digest.begin() + static_cast<std::ptrdiff_t>( result.key.size() ),
                     result.nonce.size(),
                     result.nonce.begin() );

        result.key[0] ^= static_cast<std::byte>( ( packetId >> 8 ) & 0xff );

        result.key[1] ^= static_cast<std::byte>( packetId & 0xff );

        return result;
    }

    std::array<std::byte, 32> SessionCrypto::Sha256( std::span<const std::byte> data ) {
        std::array<std::byte, 32> result {};

        std::size_t resultSize = result.size();

        if ( EVP_Q_digest( nullptr,
                           "SHA256",
                           nullptr,
                           data.data(),
                           data.size(),
                           reinterpret_cast<unsigned char*>( result.data() ),
                           &resultSize ) != 1 ) {
            throw std::runtime_error( "Failed to calculate SHA-256" );
        }

        if ( resultSize != result.size() ) {
            throw std::runtime_error( "Unexpected SHA-256 size" );
        }

        return result;
    }

    SessionEncryptedData SessionCrypto::EncryptClient( PacketType type,
                                                       std::uint16_t packetId,
                                                       std::uint32_t generationId,
                                                       std::span<const std::byte> meta,
                                                       std::span<const std::byte> data ) const {
        const KeyNonce keyNonce = CreateKeyNonce( type, true, packetId, generationId );

        const AesEax aesEax( keyNonce.key );

        AesEaxEncryptedData encrypted = aesEax.Encrypt( keyNonce.nonce, meta, data );

        SessionEncryptedData result;

        std::copy_n( encrypted.tag.begin(), result.mac.size(), result.mac.begin() );

        result.data = std::move( encrypted.data );

        return result;
    }

    std::vector<std::byte> SessionCrypto::DecryptServer( const ServerPacket& packet, std::uint32_t generationId ) const {
        const KeyNonce keyNonce = CreateKeyNonce( packet.Header().type, false, packet.Header().packetId, generationId );

        const AesEax aesEax( keyNonce.key );

        auto plaintext = aesEax.Decrypt( keyNonce.nonce, packet.Meta(), packet.Data(), packet.Header().mac );

        if ( !plaintext ) {
            throw std::runtime_error( "Invalid session packet MAC" );
        }

        return std::move( *plaintext );
    }

    const std::array<std::byte, 8>& SessionCrypto::SharedMac() const {
        return m_SharedMac;
    }

} // namespace ts::protocol
