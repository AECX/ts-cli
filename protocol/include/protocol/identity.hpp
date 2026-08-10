#ifndef TS_PROTOCOL_IDENTITY_HPP
#define TS_PROTOCOL_IDENTITY_HPP

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    class Identity {
      public:
        Identity();

        ~Identity();

        Identity( const Identity& ) = delete;

        Identity& operator=( const Identity& ) = delete;

        Identity( Identity&& other ) noexcept;

        Identity& operator=( Identity&& other ) noexcept;

        [[nodiscard]] static Identity FromPrivateKeyPem( std::string_view pem );

        [[nodiscard]] std::string PrivateKeyPem() const;

        [[nodiscard]] std::vector<std::byte> PublicKey() const;

        [[nodiscard]] std::vector<std::byte> Sign( std::span<const std::byte> data ) const;

      private:
        struct Impl;

        explicit Identity( std::unique_ptr<Impl> impl );

        std::unique_ptr<Impl> m_Impl;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_IDENTITY_HPP
