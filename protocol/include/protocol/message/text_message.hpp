#ifndef TS_PROTOCOL_MESSAGE_TEXT_MESSAGE_HPP
#define TS_PROTOCOL_MESSAGE_TEXT_MESSAGE_HPP

#include <cstdint>
#include <optional>
#include <protocol/command/command.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::protocol {

    enum class TextMessageTargetMode : std::uint8_t { Private = 1, Channel = 2, Server = 3 };

    struct TextMessageTarget {
        TextMessageTargetMode mode = TextMessageTargetMode::Private;
        std::uint64_t id = 0;
    };

    struct TextMessageEntry {
        TextMessageTargetMode targetMode = TextMessageTargetMode::Private;
        std::optional<std::uint64_t> targetId;

        std::uint16_t invokerId = 0;
        std::string invokerName;
        std::string invokerUniqueId;

        std::string text;
    };

    class TextMessage {
      public:
        [[nodiscard]] static TextMessage Parse( const Command& command );

        [[nodiscard]] const std::vector<TextMessageEntry>& Entries() const;

      private:
        [[nodiscard]] static std::uint64_t ParseUnsigned( std::string_view value, std::string_view parameterName );

        [[nodiscard]] static TextMessageTargetMode ParseTargetMode( std::string_view value );

        std::vector<TextMessageEntry> m_Entries;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_TEXT_MESSAGE_HPP
