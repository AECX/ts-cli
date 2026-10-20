#ifndef TS_CLIENT_GTK_OPTIONS_WINDOW_HPP
#define TS_CLIENT_GTK_OPTIONS_WINDOW_HPP

#include "audio_settings_page.hpp"
#include "identity_page.hpp"

#include <audio/audio_engine.hpp>
#include <config/config.hpp>
#include <filesystem>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/notebook.h>
#include <gtkmm/window.h>

namespace ts::client::gtk {

    /*
     * Modal options for the main window: audio and identity, each on its
     * own tab. While shown, the main window can't be used and the
     * microphone is muted for the server (see AudioSettingsPage).
     */
    class OptionsWindow: public Gtk::Window {
      public:
        OptionsWindow( audio::AudioEngine& audio,
                       config::Config& config,
                       std::filesystem::path identityPath,
                       AudioSettingsPage::ChangedHandler onAudioChanged,
                       IdentityPage::RenameHandler rename );

      protected:
        void on_show() override;
        void on_hide() override;

      private:
        Gtk::Box m_Layout { Gtk::Orientation::VERTICAL, 0 };
        Gtk::Notebook m_Tabs;
        AudioSettingsPage m_Audio;
        IdentityPage m_Identity;
        Gtk::Button m_Close { "Close" };
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_OPTIONS_WINDOW_HPP
