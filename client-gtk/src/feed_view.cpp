#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/gtk/feed_view.hpp>
#include <gtkmm/enums.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <vector>

namespace ts::client::gtk {

    FeedView::FeedView() {
        set_policy( Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC );
        set_has_frame( true );

        m_Container.set_orientation( Gtk::Orientation::VERTICAL );
        m_Container.set_hexpand( true );
        m_Container.set_margin( 12 );
        m_Container.set_spacing( 2 );

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
        header->set_text( "[" + line.time + "] " + line.sender + ":" );
        header->set_xalign( 0.0f );
        header->set_valign( Gtk::Align::START );
        header->set_selectable( false );

        Gtk::Label* content = Gtk::make_managed<Gtk::Label>();
        content->set_text( line.text );
        content->set_xalign( 0.0f );
        content->set_halign( Gtk::Align::FILL );
        content->set_hexpand( true );
        content->set_valign( Gtk::Align::START );
        content->set_wrap( true );
        content->set_selectable( true );

        // Header in column 0; content in column 1.
        row->attach( *header, 0, 0, 1, 1 );
        row->attach( *content, 1, 0, 1, 1 );

        m_Container.append( *row );

        /**
    switch ( line.kind ) {
        case FeedLineKind::Message:
            buffer->insert_with_tag( buffer->end(), DisplayText( line.sender + ": " ), "sender" );
            buffer->insert( buffer->end(), DisplayText( line.text ) );
            break;

        case FeedLineKind::Info:
            buffer->insert_with_tag( buffer->end(), DisplayText( line.text ), "info" );
            break;

        case FeedLineKind::Error:
            buffer->insert_with_tag( buffer->end(), DisplayText( line.text ), "error" );
            break;
    }*/
    }

} // namespace ts::client::gtk
