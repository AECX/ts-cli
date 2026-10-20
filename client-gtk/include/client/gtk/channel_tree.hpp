#ifndef TS_CLIENT_GTK_CHANNEL_TREE_HPP
#define TS_CLIENT_GTK_CHANNEL_TREE_HPP

#include "conversation_log.hpp"

#include <client/runtime/server_snapshot.hpp>
#include <cstdint>
#include <functional>
#include <giomm/liststore.h>
#include <gtkmm/listitem.h>
#include <gtkmm/listview.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/treelistmodel.h>
#include <map>
#include <optional>
#include <set>

namespace ts::client::gtk {

    /*
     * The live server/channel/client tree. Every row is a conversation:
     * selecting one switches the text feed, activating (double-clicking) a
     * channel asks to join it.
     */
    class ChannelTree: public Gtk::ListView {
      public:
        using ConversationHandler = std::function<void( const ConversationKey& )>;

        /* `area` is the row's area in this widget's coordinates, e.g. to point a popover at. */
        using ContextMenuHandler = std::function<void( const ConversationKey&, const Gdk::Rectangle& area )>;

        ChannelTree();

        void OnSelected( ConversationHandler handler );
        void OnActivated( ConversationHandler handler );
        void OnContextMenu( ContextMenuHandler handler );

        /* Rebuilds all rows from the snapshot, keeping the scroll position and the log's active conversation selected. */
        void Update( const ServerSnapshot& snapshot, const ConversationLog& log );

        /* Highlights the given clients as transmitting; updates rows in place. */
        void SetTalking( std::set<std::uint16_t> clientIds );

        /* One row of the tree; public only for the list model's sake. */
        class Item: public Glib::Object {
          public:
            static Glib::RefPtr<Item> Create( const ConversationKey& key, Glib::ustring icon, Glib::ustring markup );

            ConversationKey key;
            Glib::ustring icon;
            Glib::ustring markup;
            bool talking = false;
            Glib::RefPtr<Gio::ListStore<Item>> children;

            /* Emitted when `talking` changes, so a bound row can restyle itself. */
            sigc::signal<void()> talkingChanged;

          protected:
            Item() = default;
        };

      private:
        void SetupRow( const Glib::RefPtr<Gtk::ListItem>& listItem );
        void BindRow( const Glib::RefPtr<Gtk::ListItem>& listItem );
        void UnbindRow( const Glib::RefPtr<Gtk::ListItem>& listItem );
        void RowPressed( Gtk::ListItem& listItem, Gtk::Widget& row );
        [[nodiscard]] Glib::RefPtr<Item> ItemAt( guint position ) const;
        void SelectedChanged();
        void ApplyTalking( const Glib::RefPtr<Gio::ListStore<Item>>& items );

        Glib::RefPtr<Gio::ListStore<Item>> m_Root;
        Glib::RefPtr<Gtk::TreeListModel> m_Model;
        Glib::RefPtr<Gtk::SingleSelection> m_Selection;
        ConversationHandler m_OnSelected;
        ConversationHandler m_OnActivated;
        ContextMenuHandler m_OnContextMenu;
        std::map<Gtk::ListItem*, sigc::connection> m_RowConnections;
        std::set<std::uint16_t> m_Talking;
        bool m_Updating = false;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_CHANNEL_TREE_HPP
