#include <client/gtk/display_text.hpp>
#include <glib.h>
#include <glibmm/markup.h>
#include <string_view>

namespace ts::client::gtk {

    Glib::ustring DisplayText( std::string_view value ) {
        gchar* valid = g_utf8_make_valid( value.data(), static_cast<gssize>( value.size() ) );
        Glib::ustring result( valid );
        g_free( valid );
        return result;
    }

    Glib::ustring DisplayMarkup( std::string_view value ) {
        return Glib::Markup::escape_text( DisplayText( value ) );
    }

} // namespace ts::client::gtk
