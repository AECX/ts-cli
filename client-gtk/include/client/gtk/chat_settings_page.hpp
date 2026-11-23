#ifndef TS_CLIENT_GTK_CHAT_SETTINGS_PAGE_HPP
#define TS_CLIENT_GTK_CHAT_SETTINGS_PAGE_HPP

#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <config/config.hpp>
#include <functional>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/togglebutton.h>

namespace ts::client::gtk {

    class ChatSettingsPage: public Gtk::Box {
      public:
        ChatSettingsPage( config::Config& config );

        ChatSettingsPage( const ChatSettingsPage& ) = delete;
        ChatSettingsPage& operator=( const ChatSettingsPage& ) = delete;

      private:
        void ShowError( const std::string& message );
        void Apply( std::string_view what, const std::function<void()>& change );

        config::Config& m_Config;

        Gtk::CheckButton m_ToggleLink { "Ask before opening links" };
        Gtk::CheckButton m_ToggleImage { "Blur image previews in chat" };
        Gtk::Label m_Error;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_CHAT_SETTINGS_PAGE_HPP
