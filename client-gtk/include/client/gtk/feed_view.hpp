#ifndef TS_CLIENT_GTK_FEED_VIEW_HPP
#define TS_CLIENT_GTK_FEED_VIEW_HPP

#include "config/config.hpp"
#include "conversation_log.hpp"
#include "log/logger.hpp"

#include <cstddef>
#include <gtkmm/box.h>
#include <gtkmm/picture.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <log/logger.hpp>
#include <vector>

namespace ts::client::gtk {

    /* Read-only, auto-scrolling rendering of one conversation's lines. */
    class FeedView: public Gtk::ScrolledWindow {
      public:
        FeedView( ts::log::Logger& logger, config::Config& config );

        void Show( const std::vector<FeedLine>& lines );
        void Append( const FeedLine& line );

      private:
        ts::log::Logger& m_Logger;
        config::Config& m_Config;
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

        /**
         * @brief Parses a text message for a BBCode IMG.
         *
         * @param text The text to parse.
         * @param url Receives the image parsed URL.
         *
         * @return True if an image link was found, otherwise false.
         *         Output parameter remains unchanged if false.
         *
         * @throws Glib::Error If the underlying file operation fails.
         * @throws std::runtime_error If specifically the image loading fails.
         */
        [[nodiscard]] static bool ParseImg( const std::string& text, std::string& url );

        static void LoadImage( Gtk::Picture& picture, const std::string& url );

        Glib::RefPtr<Gtk::TextBuffer::Mark> m_End;
        Gtk::Box m_Container;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_FEED_VIEW_HPP
