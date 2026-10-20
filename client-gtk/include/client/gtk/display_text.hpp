#ifndef TS_CLIENT_GTK_DISPLAY_TEXT_HPP
#define TS_CLIENT_GTK_DISPLAY_TEXT_HPP

#include <glibmm/ustring.h>
#include <string_view>

namespace ts::client::gtk {

    /*
     * GTK requires valid UTF-8 everywhere, but names and messages come from
     * the network. Replaces invalid sequences with U+FFFD instead of letting
     * GTK drop the text with a critical warning.
     */
    [[nodiscard]] Glib::ustring DisplayText( std::string_view value );

    /* DisplayText, escaped for use inside Pango markup. */
    [[nodiscard]] Glib::ustring DisplayMarkup( std::string_view value );

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_DISPLAY_TEXT_HPP
