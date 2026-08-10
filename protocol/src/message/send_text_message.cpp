#include <cstddef>
#include <cstdint>
#include <protocol/command/writer.hpp>
#include <protocol/message/send_text_message.hpp>
#include <protocol/message/text_message.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ts::protocol {

    SendTextMessage::SendTextMessage( TextMessageTarget target, std::string text ):
        m_Target( target ), m_Text( std::move( text ) ) {
        if ( m_Text.empty() ) {
            throw std::runtime_error( "Text message is empty" );
        }

        if ( m_Target.mode != TextMessageTargetMode::Server && m_Target.id == 0 ) {
            throw std::runtime_error( "Text message target ID is zero" );
        }
    }

    std::vector<std::byte> SendTextMessage::Serialize() const {
        CommandWriter writer( "sendtextmessage" );

        writer.Write( "targetmode", static_cast<std::uint32_t>( m_Target.mode ) );
        writer.Write( "target", m_Target.id );
        writer.Write( "msg", m_Text );

        return writer.Take();
    }

} // namespace ts::protocol
