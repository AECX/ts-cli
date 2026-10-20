#include <client/gtk/display_text.hpp>
#include <client/gtk/user_audio_popover.hpp>
#include <cmath>
#include <config/user_config.hpp>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace ts::client::gtk {

    UserAudioPopover::UserAudioPopover( Gtk::Widget& relativeTo ) {
        set_parent( relativeTo );
        m_Title.set_xalign( 0.0F );
        m_VolumeLabel.set_xalign( 0.0F );

        m_Volume.set_range( static_cast<double>( config::UserConfigStore::MinVolumeDb() ),
                            static_cast<double>( config::UserConfigStore::MaxVolumeDb() ) );
        m_Volume.set_increments( 1.0, 6.0 );
        m_Volume.set_digits( 0 );
        m_Volume.set_draw_value( true );
        m_Volume.set_value_pos( Gtk::PositionType::RIGHT );
        m_Volume.add_mark( 0.0, Gtk::PositionType::BOTTOM, "0 dB" );
        m_Volume.set_size_request( 260, -1 );
        m_Volume.set_format_value_func( []( double value ) {
            std::ostringstream text;
            text << std::showpos << std::fixed << std::setprecision( 0 ) << value << " dB";
            return Glib::ustring( text.str() );
        } );

        m_Muted.set_hexpand( true );
        m_Actions.append( m_Muted );
        m_Actions.append( m_Reset );

        m_Layout.set_margin( 4 );
        m_Layout.append( m_Title );
        m_Layout.append( m_VolumeLabel );
        m_Layout.append( m_Volume );
        m_Layout.append( m_Actions );
        set_child( m_Layout );

        m_Volume.signal_value_changed().connect( sigc::mem_fun( *this, &UserAudioPopover::Changed ) );
        m_Muted.signal_toggled().connect( sigc::mem_fun( *this, &UserAudioPopover::Changed ) );
        m_Reset.signal_clicked().connect( [this] {
            m_Presenting = true;
            m_Volume.set_value( 0.0 );
            m_Muted.set_active( false );
            m_Presenting = false;
            Changed();
        } );
    }

    UserAudioPopover::~UserAudioPopover() {
        unparent();
    }

    void UserAudioPopover::OnChanged( ChangeHandler handler ) {
        m_OnChanged = std::move( handler );
    }

    void UserAudioPopover::Present( std::uint16_t clientId, const std::string& nickname, const config::UserConfig& settings ) {
        m_Presenting = true;
        m_ClientId = clientId;
        m_Title.set_markup( "<b>" + DisplayMarkup( nickname ) + "</b>" );
        m_Volume.set_value( static_cast<double>( settings.volumeDb ) );
        m_Muted.set_active( settings.muted );
        m_Presenting = false;

        popup();
    }

    void UserAudioPopover::Changed() {
        if ( m_Presenting || !m_OnChanged ) {
            return;
        }

        /* Whole dB steps keep the persisted value stable while dragging. */
        const config::UserConfig settings { .volumeDb = static_cast<float>( std::round( m_Volume.get_value() ) ),
                                            .muted = m_Muted.get_active() };
        m_OnChanged( m_ClientId, settings );
    }

} // namespace ts::client::gtk
