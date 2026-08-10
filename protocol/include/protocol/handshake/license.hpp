#ifndef TS_PROTOCOL_HANDSHAKE_LICENSE_HPP
#define TS_PROTOCOL_HANDSHAKE_LICENSE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ts::protocol {

    enum class LicenseBlockType : std::uint8_t {
        Intermediate = 0x00,
        Website = 0x01,
        Ts3Server = 0x02,
        Code = 0x03,
        Ts5Server = 0x08,
        Ephemeral = 0x20
    };

    class LicenseBlock {
      public:
        [[nodiscard]] LicenseBlockType Type() const;

        [[nodiscard]] const std::array<std::byte, 32>& PublicKey() const;

        [[nodiscard]] std::uint64_t NotValidBefore() const;

        [[nodiscard]] std::uint64_t NotValidAfter() const;

        [[nodiscard]] std::span<const std::byte> Raw() const;

        [[nodiscard]] std::span<const std::byte> HashData() const;

        [[nodiscard]] bool IsServer() const;

      private:
        [[nodiscard]] static LicenseBlock Parse( std::span<const std::byte> data, std::size_t& offset );

        LicenseBlockType m_Type = LicenseBlockType::Intermediate;

        std::array<std::byte, 32> m_PublicKey {};

        std::uint64_t m_NotValidBefore = 0;
        std::uint64_t m_NotValidAfter = 0;

        std::vector<std::byte> m_Raw;

        friend class License;
    };

    class License {
      public:
        [[nodiscard]] static License Parse( std::span<const std::byte> data );

        [[nodiscard]] std::uint8_t Version() const;

        [[nodiscard]] std::span<const std::byte> Raw() const;

        [[nodiscard]] const std::vector<LicenseBlock>& Blocks() const;

      private:
        std::uint8_t m_Version = 0;
        std::vector<std::byte> m_Raw;
        std::vector<LicenseBlock> m_Blocks;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_LICENSE_HPP
