#ifndef TS_CLIENT_GTK_IDENTITY_PAGE_HPP
#define TS_CLIENT_GTK_IDENTITY_PAGE_HPP

#include <atomic>
#include <config/config.hpp>
#include <config/identity_store.hpp>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <memory>
#include <sigc++/connection.h>
#include <string>
#include <thread>

namespace ts::client::gtk {

    /*
     * The nickname from config.conf and the one local TeamSpeak identity:
     * its unique ID, security level and key offset, plus replacing it with
     * a new or imported one, raising its security level and exporting it
     * as PEM. Everything is stored through ts-config and applies from the
     * next connection on.
     */
    class IdentityPage: public Gtk::Box {
      public:
        /* Renames the client on the server it is connected to; false when not connected. */
        using RenameHandler = std::function<bool( const std::string& nickname )>;

        IdentityPage( config::Config& config, std::filesystem::path identityPath, RenameHandler rename );
        ~IdentityPage() override;

        IdentityPage( const IdentityPage& ) = delete;
        IdentityPage& operator=( const IdentityPage& ) = delete;

        /* Reloads the nickname and identity from disk. */
        void Refresh();

        /* Stops a running security level search without saving it. */
        void StopSearch();

      private:
        void SaveNickname();
        void GenerateIdentity();
        void ImportIdentity();
        void ExportIdentity();

        void IncreaseClicked();
        void StartSearch();
        bool PollSearch();
        void SetSearching( bool searching );

        /* Asks before `replace` overwrites the identity file. */
        void ConfirmReplace( const std::string& message, std::function<void()> replace );
        void Replace( const config::LocalIdentity& identity );

        void ShowStatus( const std::string& message, bool error );
        [[nodiscard]] Gtk::Window* ParentWindow();

        config::Config& m_Config;
        std::filesystem::path m_IdentityPath;
        RenameHandler m_Rename;

        Gtk::Frame m_NicknameFrame { "Nickname" };
        Gtk::Box m_NicknameBox { Gtk::Orientation::HORIZONTAL, 6 };
        Gtk::Entry m_Nickname;
        Gtk::Button m_SaveNickname { "Save" };

        Gtk::Frame m_IdentityFrame { "Identity" };
        Gtk::Box m_IdentityBox { Gtk::Orientation::VERTICAL, 10 };
        Gtk::Grid m_Details;
        Gtk::Label m_UniqueIdLabel { "Unique ID" };
        Gtk::Label m_UniqueId;
        Gtk::Label m_SecurityLevelLabel { "Security level" };
        Gtk::Label m_SecurityLevel;
        Gtk::Label m_KeyOffsetLabel { "Key offset" };
        Gtk::Label m_KeyOffset;
        Gtk::Box m_LevelBox { Gtk::Orientation::HORIZONTAL, 6 };
        Gtk::Label m_LevelLabel { "Raise security level to" };
        Gtk::SpinButton m_Level;
        Gtk::Button m_Increase { "Increase" };
        Gtk::Label m_Progress;
        Gtk::Box m_Actions { Gtk::Orientation::HORIZONTAL, 6 };
        Gtk::Button m_Generate { "Generate new…" };
        Gtk::Button m_Import { "Import PEM…" };
        Gtk::Button m_Export { "Export PEM…" };

        Gtk::Label m_Notice;
        Gtk::Label m_Status;

        /* The search runs on m_Search over m_Searching; the GTK thread polls it through m_SearchTimer. */
        std::unique_ptr<config::LocalIdentity> m_Searching;
        std::atomic<std::uint64_t> m_SearchOffset = 0;
        std::atomic<bool> m_SearchDone = false;
        bool m_SearchFinished = false;
        std::exception_ptr m_SearchFailure;
        std::jthread m_Search;
        sigc::connection m_SearchTimer;
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_IDENTITY_PAGE_HPP
