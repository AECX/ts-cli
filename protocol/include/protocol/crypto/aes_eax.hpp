#ifndef TS_PROTOCOL_CRYPTO_AES_EAX_HPP
#define TS_PROTOCOL_CRYPTO_AES_EAX_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace ts::protocol {

    struct AesEaxEncryptedData {
        std::array<std::byte, 16> tag {};
        std::vector<std::byte> data;
    };

    class AesEax {
      public:
        explicit AesEax( const std::array<std::byte, 16>& key );

        [[nodiscard]] AesEaxEncryptedData Encrypt( std::span<const std::byte> nonce,
                                                   std::span<const std::byte> associatedData,
                                                   std::span<const std::byte> data ) const;

        [[nodiscard]] std::optional<std::vector<std::byte>> Decrypt( std::span<const std::byte> nonce,
                                                                     std::span<const std::byte> associatedData,
                                                                     std::span<const std::byte> data,
                                                                     std::span<const std::byte> tag ) const;

      private:
        [[nodiscard]] std::array<std::byte, 16> Omac( std::byte domain, std::span<const std::byte> data ) const;

        [[nodiscard]] std::array<std::byte, 16> ComputeTag( const std::array<std::byte, 16>& nonceTag,
                                                            std::span<const std::byte> associatedData,
                                                            std::span<const std::byte> ciphertext ) const;

        [[nodiscard]] std::vector<std::byte> CryptCtr( std::span<const std::byte> data,
                                                       const std::array<std::byte, 16>& counter ) const;

        std::array<std::byte, 16> m_Key {};
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_CRYPTO_AES_EAX_HPP
