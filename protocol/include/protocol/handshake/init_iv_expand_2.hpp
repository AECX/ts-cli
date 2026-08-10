#ifndef TS_PROTOCOL_HANDSHAKE_INIT_IV_EXPAND_2_HPP
#define TS_PROTOCOL_HANDSHAKE_INIT_IV_EXPAND_2_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <protocol/command/command.hpp>
#include <string>
#include <vector>

namespace ts::protocol {

    class InitIvExpand2 {
      public:
        [[nodiscard]] static InitIvExpand2 Parse( const Command& command );

        [[nodiscard]] const std::string& EncodedLicense() const;

        [[nodiscard]] const std::vector<std::byte>& License() const;

        [[nodiscard]] const std::array<std::byte, 54>& Beta() const;

        [[nodiscard]] const std::vector<std::byte>& Omega() const;

        [[nodiscard]] const std::vector<std::byte>& Proof() const;

        [[nodiscard]] std::uint32_t Ot() const;

        [[nodiscard]] const std::optional<std::string>& Root() const;

        [[nodiscard]] const std::optional<std::string>& Tvd() const;

        [[nodiscard]] const std::optional<std::string>& Time() const;

      private:
        std::string m_EncodedLicense;
        std::vector<std::byte> m_License;

        std::array<std::byte, 54> m_Beta {};

        std::vector<std::byte> m_Omega;
        std::vector<std::byte> m_Proof;

        std::uint32_t m_Ot = 0;

        std::optional<std::string> m_Root;
        std::optional<std::string> m_Tvd;
        std::optional<std::string> m_Time;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_HANDSHAKE_INIT_IV_EXPAND_2_HPP
