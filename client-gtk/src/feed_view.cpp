#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/gtk/feed_view.hpp>
#include <string>
#include <vector>

namespace ts::client::gtk {

    FeedView::FeedView() {
        set_policy( Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC );
        set_has_frame( true );

        m_Text.set_editable( false );
        m_Text.set_cursor_visible( false );
        m_Text.set_wrap_mode( Gtk::WrapMode::WORD_CHAR );
        m_Text.set_left_margin( 6 );
        m_Text.set_right_margin( 6 );

        const Glib::RefPtr<Gtk::TextBuffer> buffer = m_Text.get_buffer();
        buffer->create_tag( "time" )->property_foreground() = "#888888";
        buffer->create_tag( "sender" )->property_weight() = Pango::Weight::BOLD;
        buffer->create_tag( "info" )->property_style() = Pango::Style::ITALIC;
        buffer->create_tag( "error" )->property_foreground() = "#c01c28";
        m_End = buffer->create_mark( "end", buffer->end(), false );

        set_child( m_Text );
    }

    void FeedView::Show( const std::vector<FeedLine>& lines ) {
        m_Text.get_buffer()->set_text( "" );

        for ( const FeedLine& line : lines ) {
            Insert( line );
        }

        ScrollToEnd();
    }

    void FeedView::Append( const FeedLine& line ) {
        Insert( line );
        ScrollToEnd();
    }

    void FeedView::Insert( const FeedLine& line ) {
        const Glib::RefPtr<Gtk::TextBuffer> buffer = m_Text.get_buffer();

        if ( buffer->size() > 0 ) {
            buffer->insert( buffer->end(), "\n" );
        }

        if ( !line.time.empty() ) {
            buffer->insert_with_tag( buffer->end(), DisplayText( line.time + ' ' ), "time" );
        }

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
        }
    }

    void FeedView::ScrollToEnd() {
        const Glib::RefPtr<Gtk::TextBuffer> buffer = m_Text.get_buffer();
        buffer->move_mark( m_End, buffer->end() );
        m_Text.scroll_to( m_End );
    }

} // namespace ts::client::gtk
