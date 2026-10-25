#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/gtk/feed_view.hpp>
#include <gtkmm/cssprovider.h>
#include <gtkmm/enums.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <regex>
#include <vector>

namespace ts::client::gtk {

    namespace {

        constexpr std::string_view MessageCss = R"css(
            .chat-message,
            .chat-message:hover,
            .chat-message:focus {
                cursor: default;
                outline: none;
                box-shadow: none;
            }
        )css";
    }

    FeedView::FeedView() {
        set_policy( Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC );
        set_has_frame( true );

        m_Container.set_orientation( Gtk::Orientation::VERTICAL );
        m_Container.set_hexpand( true );
        m_Container.set_margin( 12 );
        m_Container.set_spacing( 2 );

        const Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
        css->load_from_string( std::string( MessageCss ) );
        Gtk::StyleProvider::add_provider_for_display( get_display(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

        set_child( m_Container );
    }

    void FeedView::Show( const std::vector<FeedLine>& lines ) {
        while ( Gtk::Widget* child = m_Container.get_first_child() ) {
            m_Container.remove( *child );
        }

        for ( const FeedLine& line : lines ) {
            Insert( line );
        }

        this->ScrollToEnd();
    }

    void FeedView::Append( const FeedLine& line ) {
        Insert( line );
        this->ScrollToEnd();
    }

    void FeedView::ScrollToEnd() {
        auto count = std::make_shared<int>( 0 );

        add_tick_callback( [this, count]( const Glib::RefPtr<Gdk::FrameClock>& ) -> bool {
            if ( ++( *count ) < 2 )
                return true; // Wait for another frame.

            auto adj = get_vadjustment();

            adj->set_value( adj->get_upper() - adj->get_page_size() );

            return false;
        } );
    }

    void FeedView::Insert( const FeedLine& line ) {
        Gtk::Grid* row = Gtk::make_managed<Gtk::Grid>();
        row->set_hexpand( true );
        row->set_column_spacing( 8 );

        Gtk::Label* header = Gtk::make_managed<Gtk::Label>();

        switch ( line.kind ) {
            case FeedLineKind::Message:
                header->set_text( "[" + line.time + "] " + line.sender + ":" );
                break;

            case FeedLineKind::Info:
                header->set_text( "[" + line.time + "] <info>:" );
                break;

            case FeedLineKind::Error:
                header->set_text( "[" + line.time + "] <error>:" );
                break;
        }

        header->set_xalign( 0.0f );
        header->set_valign( Gtk::Align::START );
        header->set_selectable( false );

        Gtk::Label* content = Gtk::make_managed<Gtk::Label>();

        std::string label, url;
        if ( FeedView::ParseUrl( line.text, label, url ) ) {
            content->set_markup( "<a href=\"" + url + "\">" + label + "</a>" );
        } else {
            content->set_text( line.text );
        }
        content->add_css_class( "chat-message" );
        content->set_xalign( 0.0f );
        content->set_halign( Gtk::Align::FILL );
        content->set_hexpand( true );
        content->set_valign( Gtk::Align::START );
        content->set_wrap( true );
        content->set_selectable( true );
        content->set_focusable( false );

        // Header in column 0; content in column 1.
        row->attach( *header, 0, 0, 1, 1 );
        row->attach( *content, 1, 0, 1, 1 );

        m_Container.append( *row );
    }

    bool FeedView::ParseUrl( const std::string& text, std::string& label, std::string& url ) {
        static const std::regex pattern( R"(\[url=(.*?)\](.*?)\[/url\])", std::regex::icase );

        std::smatch match;
        if ( std::regex_search( text, match, pattern ) ) {
            url = match[1].str();
            label = match[2].str();
        } else {
            url = label = text;
        }

        return url.starts_with( "http://" ) || url.starts_with( "https://" );
    }

} // namespace ts::client::gtk
