#include <audio/audio_engine.hpp>
#include <client/gtk/options_window.hpp>
#include <config/config.hpp>
#include <filesystem>
#include <utility>

namespace ts::client::gtk {

    OptionsWindow::OptionsWindow( audio::AudioEngine& audio,
                                  config::Config& config,
                                  std::filesystem::path identityPath,
                                  AudioSettingsPage::ChangedHandler onAudioChanged,
                                  IdentityPage::RenameHandler rename ):
        m_Audio( audio, std::move( onAudioChanged ) ), m_Identity( config, std::move( identityPath ), std::move( rename ) ) {
        set_title( "Options" );
        set_default_size( 480, -1 );
        set_resizable( false );
        set_modal( true );
        set_hide_on_close( true );

        m_Tabs.append_page( m_Audio, "Audio" );
        m_Tabs.append_page( m_Identity, "Identity" );

        m_Close.set_halign( Gtk::Align::END );
        m_Close.set_margin( 12 );
        m_Close.signal_clicked().connect( [this] {
            set_visible( false );
        } );

        m_Layout.append( m_Tabs );
        m_Layout.append( m_Close );
        set_child( m_Layout );
    }

    void OptionsWindow::on_show() {
        m_Audio.Open();
        m_Identity.Refresh();
        Gtk::Window::on_show();
    }

    void OptionsWindow::on_hide() {
        m_Audio.Close();
        m_Identity.StopSearch();
        Gtk::Window::on_hide();
    }

} // namespace ts::client::gtk
