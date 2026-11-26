#include "config/config.hpp"
#include "log/logger.hpp"

#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/gtk/feed_view.hpp>
#include <giomm/file.h>
#include <glib-object.h>
#include <glibmm.h>
#include <glibmm/error.h>
#include <glibmm/markup.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/enums.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/object.h>
#include <gtkmm/picture.h>
#include <regex>
#include <stdexcept>
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
			.blur {
				filter: blur(4px);
			}
        )css";
    }

    FeedView::FeedView( ts::log::Logger& logger, config::Config& config ): m_Logger( logger ), m_Config( config ) {
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
        ScrollToEnd();
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

        Gtk::Widget* content;
        std::string link_label, url;

        if ( FeedView::ParseImg( line.text, url ) ) {
            Gtk::Picture* picture = Gtk::make_managed<Gtk::Picture>();
            content = picture;

            picture->set_halign( Gtk::Align::START );
            picture->set_hexpand( false );
            picture->set_valign( Gtk::Align::START );
            picture->set_can_shrink( false );
            picture->set_content_fit( Gtk::ContentFit::CONTAIN );

            if ( m_Config.ChatConfig().blurImages ) {
                picture->add_css_class( "blur" );
                auto motion = Gtk::EventControllerMotion::create();
                motion->signal_enter().connect(
                    [picture]( double, double ) {
                        picture->remove_css_class( "blur" );
                    },
                    false );

                motion->signal_leave().connect(
                    [picture]() {
                        picture->add_css_class( "blur" );
                    },
                    false );

                picture->add_controller( motion );
            }

            try {
                FeedView::LoadImage( *picture, url );
            } catch ( const Glib::Error& error ) {
                m_Logger.Error( "client", error.what() );
            } catch ( const std::runtime_error& error ) {
                m_Logger.Warning( "client", error.what() );
            }

        } else {
            Gtk::Label* label = Gtk::make_managed<Gtk::Label>();
            content = label;

            if ( FeedView::ParseUrl( line.text, link_label, url ) ) {
                label->set_markup( "<a href=\"" + Glib::Markup::escape_text( url ) + "\">" +
                                   Glib::Markup::escape_text( link_label ) + "</a>" );

                label->signal_activate_link().connect(
                    [this]( const Glib::ustring& uri ) -> bool {
                        auto* parent = dynamic_cast<Gtk::Window*>( get_root() );
                        auto* dialog = new Gtk::MessageDialog( "Do you want to open this link?\n" + uri,
                                                               false,
                                                               Gtk::MessageType::QUESTION,
                                                               Gtk::ButtonsType::YES_NO,
                                                               true );

                        // Check here to reflect the current state
                        // at the time of clicking regardless of
                        // when the message was created.
                        if ( !m_Config.ChatConfig().warnOpenLinks ) {
                            return false;
                        }

                        if ( parent ) {
                            dialog->set_transient_for( *parent );
                        }

                        dialog->set_modal( true );

                        dialog->signal_response().connect( [dialog, uri]( int response ) {
                            if ( response == Gtk::ResponseType::YES ) {
                                Gio::AppInfo::launch_default_for_uri( uri );
                            }

                            delete dialog;
                        } );
                        dialog->present();
                        return true;
                    },
                    false );

            } else {
                label->set_text( line.text );
            }
            label->add_css_class( "chat-message" );
            label->set_xalign( 0.0f );
            label->set_wrap( true );
            label->set_selectable( true );
            label->set_halign( Gtk::Align::FILL );
            label->set_hexpand( true );
            label->set_focusable( false );
        }

        // Header in column 0; content in column 1.
        row->attach( *header, 0, 0, 1, 1 );
        row->attach( *content, 1, 0, 1, 1 );

        m_Container.append( *row );
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

    bool FeedView::ParseUrl( const std::string& text, std::string& label, std::string& url ) {
        static const std::regex pattern( R"(\[url(?:=([^\]]+))?\]([^\[]+)\[/url\])", std::regex_constants::icase );

        std::smatch match;
        if ( std::regex_search( text, match, pattern ) ) {
            url = match[1].matched ? match[1].str() : match[2].str();
            label = match[2].str();
        } else {
            url = label = text;
        }

        return url.starts_with( "http://" ) || url.starts_with( "https://" );
    }

    bool FeedView::ParseImg( const std::string& text, std::string& url ) {
        static const std::regex pattern( R"(\[img\](.*?)\[/img\])", std::regex::icase );

        std::smatch match;

        if ( std::regex_search( text, match, pattern ) ) {
            url = match[1].str();
        }

        return url.starts_with( "https://" ) || url.starts_with( "http://" );
    }

    void FeedView::LoadImage( Gtk::Picture& picture, const std::string& url ) {
        struct Request {
            Glib::RefPtr<Gio::File> file;
            GWeakRef picture;
        };

        auto* request = new Request {};
        request->file = Gio::File::create_for_uri( url );

        g_weak_ref_init( &request->picture, G_OBJECT( picture.gobj() ) );

        request->file->load_contents_async( [request, url]( const Glib::RefPtr<Gio::AsyncResult>& result ) {
            char* contents = nullptr;
            gsize length = 0;

            if ( !request->file->load_contents_finish( result, contents, length ) ) {
                throw std::runtime_error( "Failed to load image " + url );
            } else {
                auto bytes = Glib::Bytes::create( contents, length );

                auto texture = Gdk::Texture::create_from_bytes( bytes );

                GObject* object = static_cast<GObject*>( g_weak_ref_get( &request->picture ) );

                if ( object ) {
                    gtk_picture_set_paintable( GTK_PICTURE( object ), GDK_PAINTABLE( texture->gobj() ) );
                    g_object_unref( object );
                }
            }

            g_free( contents );

            g_weak_ref_clear( &request->picture );
            delete request;
        } );
    }

} // namespace ts::client::gtk
