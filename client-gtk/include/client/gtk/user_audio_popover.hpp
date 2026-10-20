#ifndef TS_CLIENT_GTK_USER_AUDIO_POPOVER_HPP
#define TS_CLIENT_GTK_USER_AUDIO_POPOVER_HPP

#include <config/user_config.hpp>
#include <cstdint>
#include <functional>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/popover.h>
#include <gtkmm/scale.h>
#include <string>

namespace ts::client::gtk {

    /*
     * Context popover for one remote client: local playback volume and
     * local mute. Every change is reported immediately, so the effect is
     * audible while dragging the slider.
     */
    class UserAudioPopover: public Gtk::Popover {
      public:
        using ChangeHandler = std::function<void( std::uint16_t clientId, const config::UserConfig& settings )>;

        /* Shown on top of `relativeTo`; set_pointing_to() is relative to it. */
        explicit UserAudioPopover( Gtk::Widget& relativeTo );
        ~UserAudioPopover() override;

        UserAudioPopover( const UserAudioPopover& ) = delete;
        UserAudioPopover& operator=( const UserAudioPopover& ) = delete;

        void OnChanged( ChangeHandler handler );
        void Present( std::uint16_t clientId, const std::string& nickname, const config::UserConfig& settings );

      private:
        void Changed();

        Gtk::Box m_Layout { Gtk::Orientation::VERTICAL, 6 };
        Gtk::Label m_Title;
        Gtk::Label m_VolumeLabel { "Volume" };
        Gtk::Scale m_Volume { Gtk::Orientation::HORIZONTAL };
        Gtk::Box m_Actions { Gtk::Orientation::HORIZONTAL, 6 };
        Gtk::CheckButton m_Muted { "Mute locally" };
        Gtk::Button m_Reset { "Reset" };

        ChangeHandler m_OnChanged;
        std::uint16_t m_ClientId = 0;
        bool m_Presenting = false;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_USER_AUDIO_POPOVER_HPP
