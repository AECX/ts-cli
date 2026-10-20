#include <algorithm>
#include <client/cli/channel_tree_view.hpp>
#include <client/gtk/channel_tree.hpp>
#include <client/gtk/conversation_log.hpp>
#include <client/gtk/display_text.hpp>
#include <client/runtime/server_snapshot.hpp>
#include <cstddef>
#include <cstdint>
#include <glibmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/treeexpander.h>
#include <map>
#include <memory>
#include <optional>
#include <protocol/state/client.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::client::gtk {

    namespace {

        /*
         * The talk indicator in front of every client: an empty ring while
         * silent, filled green while transmitting. Drawn by CSS rather than
         * taken from a font so both states have the same width on every platform.
         */
        constexpr std::string_view IndicatorCss = R"css(
            .talk-indicator {
                min-width: 9px;
                min-height: 9px;
                margin: 0 4px 0 2px;
                border-radius: 999px;
                border: 1.5px solid alpha(#808080, 0.8);
            }

            .talk-indicator.talking {
                background-color: #2ec27e;
                border-color: #26a269;
            }
        )css";

        /* The row's widgets, created once per recycled row in SetupRow(). */
        struct RowWidgets {
            Gtk::TreeExpander* expander = nullptr;
            Gtk::Box* indicator = nullptr;
            Gtk::Image* icon = nullptr;
            Gtk::Label* label = nullptr;
        };

        [[nodiscard]] RowWidgets WidgetsOf( Gtk::ListItem& listItem ) {
            auto* expander = dynamic_cast<Gtk::TreeExpander*>( listItem.get_child() );
            auto* box = dynamic_cast<Gtk::Box*>( expander->get_child() );
            auto* indicator = dynamic_cast<Gtk::Box*>( box->get_first_child() );
            auto* icon = dynamic_cast<Gtk::Image*>( indicator->get_next_sibling() );
            auto* label = dynamic_cast<Gtk::Label*>( icon->get_next_sibling() );
            return RowWidgets { .expander = expander, .indicator = indicator, .icon = icon, .label = label };
        }

        void StyleIndicator( Gtk::Box& indicator, bool talking ) {
            if ( talking ) {
                indicator.add_css_class( "talking" );
            } else {
                indicator.remove_css_class( "talking" );
            }
        }

        [[nodiscard]] Glib::ustring UnreadSuffix( std::size_t unread ) {
            if ( unread == 0 ) {
                return {};
            }

            return " <span weight=\"bold\" foreground=\"#3584e4\">(" + std::to_string( unread ) + ")</span>";
        }

        [[nodiscard]] Glib::ustring ClientIcon( const protocol::Client& client ) {
            if ( client.outputMuted || !client.outputHardware ) {
                return "audio-volume-muted-symbolic";
            }

            if ( client.inputMuted || !client.inputHardware ) {
                return "microphone-sensitivity-muted-symbolic";
            }

            if ( client.away ) {
                return "user-away-symbolic";
            }

            return "avatar-default-symbolic";
        }

    } // namespace

    Glib::RefPtr<ChannelTree::Item>
        ChannelTree::Item::Create( const ConversationKey& key, Glib::ustring icon, Glib::ustring markup ) {
        Glib::RefPtr<Item> item = Glib::make_refptr_for_instance<Item>( new Item() );
        item->key = key;
        item->icon = std::move( icon );
        item->markup = std::move( markup );
        item->children = Gio::ListStore<Item>::create();
        return item;
    }

    ChannelTree::ChannelTree(): m_Root( Gio::ListStore<Item>::create() ) {
        m_Model = Gtk::TreeListModel::create(
            m_Root,
            []( const Glib::RefPtr<Glib::ObjectBase>& object ) -> Glib::RefPtr<Gio::ListModel> {
                const auto item = std::dynamic_pointer_cast<Item>( object );

                if ( !item || item->children->get_n_items() == 0 ) {
                    return {};
                }

                return item->children;
            },
            false,
            true );
        m_Selection = Gtk::SingleSelection::create( m_Model );
        m_Selection->set_autoselect( false );
        m_Selection->set_can_unselect( true );
        set_model( m_Selection );

        const Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
        css->load_from_string( std::string( IndicatorCss ) );
        Gtk::StyleProvider::add_provider_for_display( get_display(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

        const Glib::RefPtr<Gtk::SignalListItemFactory> factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect( sigc::mem_fun( *this, &ChannelTree::SetupRow ) );
        factory->signal_bind().connect( sigc::mem_fun( *this, &ChannelTree::BindRow ) );
        factory->signal_unbind().connect( sigc::mem_fun( *this, &ChannelTree::UnbindRow ) );
        set_factory( factory );

        m_Selection->signal_selection_changed().connect( [this]( guint, guint ) {
            SelectedChanged();
        } );

        signal_activate().connect( [this]( guint position ) {
            const Glib::RefPtr<Item> item = ItemAt( position );

            if ( item && m_OnActivated ) {
                m_OnActivated( item->key );
            }
        } );
    }

    void ChannelTree::OnSelected( ConversationHandler handler ) {
        m_OnSelected = std::move( handler );
    }

    void ChannelTree::OnActivated( ConversationHandler handler ) {
        m_OnActivated = std::move( handler );
    }

    void ChannelTree::OnContextMenu( ContextMenuHandler handler ) {
        m_OnContextMenu = std::move( handler );
    }

    void ChannelTree::Clear() {
        m_Updating = true;
        m_Talking.clear();
        m_Root->remove_all();
        m_Updating = false;
    }

    void ChannelTree::SetTalking( std::set<std::uint16_t> clientIds ) {
        m_Talking = std::move( clientIds );
        ApplyTalking( m_Root );
    }

    void ChannelTree::Update( const ServerSnapshot& snapshot, const ConversationLog& log ) {
        m_Updating = true;

        const Glib::RefPtr<Gtk::Adjustment> scroll = get_vadjustment();
        const double scrollPosition = scroll ? scroll->get_value() : 0.0;

        std::map<std::uint64_t, std::vector<const protocol::Client*>> clientsByChannel;

        for ( const protocol::Client& client : snapshot.clients ) {
            clientsByChannel[client.channelId].push_back( &client );
        }

        for ( auto& [channelId, clients] : clientsByChannel ) {
            std::sort( clients.begin(), clients.end(), []( const protocol::Client* left, const protocol::Client* right ) {
                return left->nickname < right->nickname;
            } );
        }

        const Glib::RefPtr<Item> server =
            Item::Create( ServerConversation(),
                          "network-server-symbolic",
                          "<b>" + DisplayMarkup( snapshot.serverName.empty() ? std::string( "Server" ) : snapshot.serverName ) +
                              "</b>" + UnreadSuffix( log.Unread( ServerConversation() ) ) );

        /* parents[d] is the most recent channel item at depth d. */
        std::vector<Glib::RefPtr<Item>> parents;

        for ( const ChannelSnapshot& entry : snapshot.channels ) {
            const bool nested = entry.depth > 0 && entry.depth <= parents.size();
            const Glib::RefPtr<Item>& parent = nested ? parents[entry.depth - 1] : server;

            const ConversationKey key = ChannelConversation( entry.channel.id );
            Glib::ustring markup = DisplayMarkup( entry.channel.name );

            if ( entry.channel.id == snapshot.currentChannelId ) {
                markup = "<b>" + markup + "</b>";
            }

            const Glib::RefPtr<Item> channel =
                Item::Create( key,
                              entry.channel.passwordProtected ? "channel-secure-symbolic" : "folder-symbolic",
                              markup + UnreadSuffix( log.Unread( key ) ) );

            const auto clients = clientsByChannel.find( entry.channel.id );

            if ( clients != clientsByChannel.end() ) {
                for ( const protocol::Client* client : clients->second ) {
                    const ConversationKey clientKey = ClientConversation( client->id );
                    Glib::ustring clientMarkup =
                        DisplayMarkup( client->nickname.empty() ? "client " + std::to_string( client->id ) : client->nickname );

                    if ( client->id == snapshot.clientId ) {
                        clientMarkup = "<b>" + clientMarkup + "</b>";
                    }

                    clientMarkup += "<span alpha=\"55%\">" +
                                    DisplayMarkup( cli::FormatClientStatus( *client, snapshot.clientId ) ) + "</span>" +
                                    UnreadSuffix( log.Unread( clientKey ) );

                    channel->children->append( Item::Create( clientKey, ClientIcon( *client ), clientMarkup ) );
                }
            }

            parent->children->append( channel );
            parents.resize( std::min( entry.depth, parents.size() ) );
            parents.push_back( channel );
        }

        m_Root->remove_all();
        m_Root->append( server );
        ApplyTalking( m_Root );

        m_Selection->unselect_all();

        for ( guint position = 0; position < m_Selection->get_n_items(); ++position ) {
            const Glib::RefPtr<Item> item = ItemAt( position );

            if ( item && item->key == log.Active() ) {
                m_Selection->set_selected( position );
                break;
            }
        }

        if ( scroll ) {
            /* The adjustment's range is only recomputed on the next size allocation. */
            Glib::signal_idle().connect_once( [scroll, scrollPosition] {
                scroll->set_value( scrollPosition );
            } );
        }

        m_Updating = false;
    }

    void ChannelTree::SetupRow( const Glib::RefPtr<Gtk::ListItem>& listItem ) {
        auto* expander = Gtk::make_managed<Gtk::TreeExpander>();
        auto* box = Gtk::make_managed<Gtk::Box>( Gtk::Orientation::HORIZONTAL, 4 );
        auto* indicator = Gtk::make_managed<Gtk::Box>();
        auto* icon = Gtk::make_managed<Gtk::Image>();
        auto* label = Gtk::make_managed<Gtk::Label>();

        indicator->add_css_class( "talk-indicator" );
        indicator->set_valign( Gtk::Align::CENTER );
        label->set_xalign( 0.0F );
        label->set_ellipsize( Pango::EllipsizeMode::END );
        label->set_hexpand( true );

        box->append( *indicator );
        box->append( *icon );
        box->append( *label );
        expander->set_child( *box );

        /* The list item outlives its child and therefore this gesture; a raw pointer avoids a reference cycle. */
        Gtk::ListItem* item = listItem.get();
        const Glib::RefPtr<Gtk::GestureClick> click = Gtk::GestureClick::create();
        click->set_button( GDK_BUTTON_SECONDARY );
        click->signal_pressed().connect( [this, item, expander]( int, double, double ) {
            RowPressed( *item, *expander );
        } );
        expander->add_controller( click );

        listItem->set_child( *expander );
    }

    void ChannelTree::BindRow( const Glib::RefPtr<Gtk::ListItem>& listItem ) {
        const auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>( listItem->get_item() );
        const auto item = row ? std::dynamic_pointer_cast<Item>( row->get_item() ) : Glib::RefPtr<Item> {};

        if ( !item ) {
            return;
        }

        const RowWidgets widgets = WidgetsOf( *listItem );
        widgets.expander->set_list_row( row );
        widgets.icon->set_from_icon_name( item->icon );
        widgets.label->set_markup( item->markup );
        widgets.indicator->set_visible( item->key.kind == ConversationKind::Client );
        StyleIndicator( *widgets.indicator, item->talking );

        Gtk::Box* indicator = widgets.indicator;
        Item* raw = item.get();
        m_RowConnections[listItem.get()] = item->talkingChanged.connect( [indicator, raw] {
            StyleIndicator( *indicator, raw->talking );
        } );
    }

    void ChannelTree::UnbindRow( const Glib::RefPtr<Gtk::ListItem>& listItem ) {
        const auto connection = m_RowConnections.find( listItem.get() );

        if ( connection != m_RowConnections.end() ) {
            connection->second.disconnect();
            m_RowConnections.erase( connection );
        }

        WidgetsOf( *listItem ).expander->set_list_row( {} );
    }

    void ChannelTree::RowPressed( Gtk::ListItem& listItem, Gtk::Widget& row ) {
        const Glib::RefPtr<Item> item = ItemAt( listItem.get_position() );

        if ( !item || !m_OnContextMenu ) {
            return;
        }

        /* Right-clicking also selects, so the feed follows the row the menu is for. */
        m_Selection->set_selected( listItem.get_position() );

        const std::optional<Gdk::Graphene::Rect> bounds = row.compute_bounds( *this );

        if ( !bounds ) {
            return;
        }

        const Gdk::Rectangle area( static_cast<int>( bounds->get_x() ),
                                   static_cast<int>( bounds->get_y() ),
                                   static_cast<int>( bounds->get_width() ),
                                   static_cast<int>( bounds->get_height() ) );
        m_OnContextMenu( item->key, area );
    }

    Glib::RefPtr<ChannelTree::Item> ChannelTree::ItemAt( guint position ) const {
        if ( position >= m_Selection->get_n_items() ) {
            return {};
        }

        const auto row = std::dynamic_pointer_cast<Gtk::TreeListRow>( m_Selection->get_object( position ) );
        return row ? std::dynamic_pointer_cast<Item>( row->get_item() ) : Glib::RefPtr<Item> {};
    }

    void ChannelTree::SelectedChanged() {
        if ( m_Updating || !m_OnSelected ) {
            return;
        }

        const Glib::RefPtr<Item> item = ItemAt( m_Selection->get_selected() );

        if ( item ) {
            m_OnSelected( item->key );
        }
    }

    void ChannelTree::ApplyTalking( const Glib::RefPtr<Gio::ListStore<Item>>& items ) {
        for ( guint position = 0; position < items->get_n_items(); ++position ) {
            const Glib::RefPtr<Item> item = items->get_item( position );

            if ( item->key.kind == ConversationKind::Client ) {
                const bool talking = m_Talking.contains( static_cast<std::uint16_t>( item->key.id ) );

                if ( item->talking != talking ) {
                    item->talking = talking;
                    item->talkingChanged.emit();
                }
            }

            ApplyTalking( item->children );
        }
    }

} // namespace ts::client::gtk
