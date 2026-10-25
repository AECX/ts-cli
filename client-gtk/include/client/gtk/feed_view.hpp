#ifndef TS_CLIENT_GTK_FEED_VIEW_HPP
#define TS_CLIENT_GTK_FEED_VIEW_HPP

#include "conversation_log.hpp"

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <vector>

namespace ts::client::gtk {

    /* Read-only, auto-scrolling rendering of one conversation's lines. */
    class FeedView: public Gtk::ScrolledWindow {
      public:
        FeedView();

        void Show( const std::vector<FeedLine>& lines );
        void Append( const FeedLine& line );

      private:
        void Insert( const FeedLine& line );
        void ScrollToEnd();

        /**
         * @brief Parses a text message for a BBCode URL.
         *
         * @param text The text to parse.
         * @param label Receives the link label, or the URL if no label is specified.
         * @param url Receives the parsed URL.
         *
         * @return True if a link was found, otherwise false.
         *         Output parameters are equal to text when no link is found.
         */
        [[nodiscard]] static bool ParseUrl( const std::string& text, std::string& label, std::string& url );

        Glib::RefPtr<Gtk::TextBuffer::Mark> m_End;
        Gtk::Box m_Container;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_FEED_VIEW_HPP
