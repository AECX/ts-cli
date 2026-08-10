#ifndef TS_PROTOCOL_MESSAGE_SEND_TEXT_MESSAGE_HPP
#define TS_PROTOCOL_MESSAGE_SEND_TEXT_MESSAGE_HPP

#include "text_message.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ts::protocol {

    class SendTextMessage {
      public:
        SendTextMessage( TextMessageTarget target, std::string text );

        [[nodiscard]] std::vector<std::byte> Serialize() const;

      private:
        TextMessageTarget m_Target;
        std::string m_Text;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_MESSAGE_SEND_TEXT_MESSAGE_HPP
