#include "config/chat_settings.hpp"

#include <audio/audio_types.hpp>
#include <client/gtk/chat_settings_page.hpp>
#include <client/gtk/display_text.hpp>
#include <config/config.hpp>
#include <exception>
#include <functional>
#include <glibmm/main.h>
#include <string>
#include <string_view>

namespace ts::client::gtk {

    ChatSettingsPage::ChatSettingsPage( config::Config& config ):
        Gtk::Box( Gtk::Orientation::VERTICAL, 10 ), m_Config( config ) {

        auto chat = config.ChatConfig();
        m_ToggleImage.set_active( chat.blurImages );
        m_ToggleLink.set_active( chat.warnOpenLinks );

        append( m_ToggleImage );
        append( m_ToggleLink );

        m_Error.set_xalign( 0.0F );
        m_Error.set_wrap( true );
        m_Error.set_visible( false );

        set_margin( 12 );
        append( m_Error );

        auto handler = [this] {
            Apply( "image blur", [this] {
                m_Config.Update( [this]( config::Config& config ) {
                    config.SetChatConfig(
                        { .warnOpenLinks = m_ToggleLink.get_active(), .blurImages = m_ToggleImage.get_active() } );
                } );
            } );
        };

        m_ToggleImage.signal_toggled().connect( handler );
        m_ToggleLink.signal_toggled().connect( handler );
    }
    void ChatSettingsPage::Apply( std::string_view what, const std::function<void()>& change ) {
        try {
            change();
            m_Error.set_visible( false );
        } catch ( const std::exception& exception ) {
            ShowError( "Could not change " + std::string( what ) + ": " + exception.what() );
        }
    }

    void ChatSettingsPage::ShowError( const std::string& message ) {
        m_Error.set_markup( "<span foreground=\"#c01c28\">" + DisplayMarkup( message ) + "</span>" );
        m_Error.set_visible( true );
    }

} // namespace ts::client::gtk
