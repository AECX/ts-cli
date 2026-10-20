#include <algorithm>
#include <client/gtk/display_text.hpp>
#include <client/gtk/identity_page.hpp>
#include <config/config.hpp>
#include <config/identity_store.hpp>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <giomm/file.h>
#include <glibmm/error.h>
#include <glibmm/main.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/window.h>
#include <initializer_list>
#include <memory>
#include <protocol/identity.hpp>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace ts::client::gtk {

    IdentityPage::IdentityPage( config::Config& config, std::filesystem::path identityPath, RenameHandler rename ):
        Gtk::Box( Gtk::Orientation::VERTICAL, 12 ), m_Config( config ), m_IdentityPath( std::move( identityPath ) ),
        m_Rename( std::move( rename ) ) {
        set_margin( 12 );

        m_Nickname.set_hexpand( true );
        m_Nickname.set_tooltip_text( "The nickname you join servers with" );
        m_NicknameBox.set_margin( 8 );
        m_NicknameBox.append( m_Nickname );
        m_NicknameBox.append( m_SaveNickname );
        m_NicknameFrame.set_child( m_NicknameBox );

        for ( Gtk::Label* label : { &m_UniqueIdLabel, &m_SecurityLevelLabel, &m_KeyOffsetLabel } ) {
            label->set_xalign( 0.0F );
        }

        for ( Gtk::Label* value : { &m_UniqueId, &m_SecurityLevel, &m_KeyOffset } ) {
            value->set_xalign( 0.0F );
            value->set_selectable( true );
            value->set_hexpand( true );
        }

        m_UniqueId.set_ellipsize( Pango::EllipsizeMode::MIDDLE );
        m_Details.set_row_spacing( 6 );
        m_Details.set_column_spacing( 12 );
        m_Details.attach( m_UniqueIdLabel, 0, 0 );
        m_Details.attach( m_UniqueId, 1, 0 );
        m_Details.attach( m_SecurityLevelLabel, 0, 1 );
        m_Details.attach( m_SecurityLevel, 1, 1 );
        m_Details.attach( m_KeyOffsetLabel, 0, 2 );
        m_Details.attach( m_KeyOffset, 1, 2 );

        m_Level.set_range( 0, config::IdentityStore::MaxSecurityLevel() );
        m_Level.set_increments( 1, 4 );
        m_Level.set_numeric( true );
        m_Increase.set_tooltip_text( "Search for a key offset reaching the higher level. Some servers require a minimum "
                                     "level. Each level takes about twice as long; you can stop at any time." );
        m_LevelBox.append( m_LevelLabel );
        m_LevelBox.append( m_Level );
        m_LevelBox.append( m_Increase );
        m_Progress.set_xalign( 0.0F );
        m_Progress.set_visible( false );

        m_Generate.set_tooltip_text( "Replace your identity with a newly generated one" );
        m_Import.set_tooltip_text( "Replace your identity with a private key from a PEM file" );
        m_Export.set_tooltip_text( "Save your identity's private key as a PEM file, e.g. as a backup" );
        m_Actions.append( m_Generate );
        m_Actions.append( m_Import );
        m_Actions.append( m_Export );

        m_IdentityBox.set_margin( 8 );
        m_IdentityBox.append( m_Details );
        m_IdentityBox.append( m_LevelBox );
        m_IdentityBox.append( m_Progress );
        m_IdentityBox.append( m_Actions );
        m_IdentityFrame.set_child( m_IdentityBox );

        m_Notice.set_markup( "<small><i>Identity changes apply the next time you connect. Servers recognize you by your "
                             "identity: replacing it makes you a new user to them.</i></small>" );
        m_Notice.set_xalign( 0.0F );
        m_Notice.set_wrap( true );

        m_Status.set_xalign( 0.0F );
        m_Status.set_wrap( true );
        m_Status.set_visible( false );

        append( m_NicknameFrame );
        append( m_IdentityFrame );
        append( m_Notice );
        append( m_Status );

        m_Nickname.signal_activate().connect( sigc::mem_fun( *this, &IdentityPage::SaveNickname ) );
        m_SaveNickname.signal_clicked().connect( sigc::mem_fun( *this, &IdentityPage::SaveNickname ) );
        m_Generate.signal_clicked().connect( sigc::mem_fun( *this, &IdentityPage::GenerateIdentity ) );
        m_Import.signal_clicked().connect( sigc::mem_fun( *this, &IdentityPage::ImportIdentity ) );
        m_Export.signal_clicked().connect( sigc::mem_fun( *this, &IdentityPage::ExportIdentity ) );
        m_Increase.signal_clicked().connect( sigc::mem_fun( *this, &IdentityPage::IncreaseClicked ) );
    }

    IdentityPage::~IdentityPage() {
        StopSearch();
    }

    void IdentityPage::Refresh() {
        m_Status.set_visible( false );
        m_Nickname.set_text( DisplayText( m_Config.Profile().nickname ) );

        try {
            const config::LocalIdentity identity = config::IdentityStore::Load( m_IdentityPath );
            m_UniqueId.set_text( identity.identity.UniqueId() );
            m_SecurityLevel.set_text( std::to_string( identity.securityLevel ) );
            m_KeyOffset.set_text( std::to_string( identity.keyOffset ) );
            m_Export.set_sensitive( true );

            const bool raisable = identity.securityLevel < config::IdentityStore::MaxSecurityLevel();
            m_Level.set_range( std::min<double>( identity.securityLevel + 1, config::IdentityStore::MaxSecurityLevel() ),
                               config::IdentityStore::MaxSecurityLevel() );
            m_Level.set_value( identity.securityLevel + 1 );
            m_LevelBox.set_sensitive( raisable );
        } catch ( const std::exception& exception ) {
            for ( Gtk::Label* value : { &m_UniqueId, &m_SecurityLevel, &m_KeyOffset } ) {
                value->set_text( "-" );
            }

            m_Export.set_sensitive( false );
            m_LevelBox.set_sensitive( false );
            ShowStatus( std::string( "Could not read your identity: " ) + exception.what(), true );
        }
    }

    void IdentityPage::SaveNickname() {
        const std::string nickname = m_Nickname.get_text();

        try {
            m_Config.Update( [&nickname]( config::Config& config ) {
                config.SetNickname( nickname );
            } );
            ShowStatus( m_Rename && m_Rename( nickname ) ? "Nickname saved and changed on the server."
                                                         : "Nickname saved; it is used from the next connection on.",
                        false );
        } catch ( const std::exception& exception ) {
            ShowStatus( std::string( "Could not save the nickname: " ) + exception.what(), true );
        }
    }

    void IdentityPage::GenerateIdentity() {
        ConfirmReplace( "Generate a new identity?", [this] {
            Replace( config::IdentityStore::Create( protocol::Identity {}, config::IdentityStore::DefaultSecurityLevel() ) );
        } );
    }

    void IdentityPage::ImportIdentity() {
        Gtk::Window* parent = ParentWindow();

        if ( parent == nullptr ) {
            return;
        }

        const Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
        dialog->set_title( "Import identity" );
        dialog->open( *parent, [this, dialog]( const Glib::RefPtr<Gio::AsyncResult>& result ) {
            std::filesystem::path path;

            try {
                path = dialog->open_finish( result )->get_path();
            } catch ( const Glib::Error& ) {
                return; /* Cancelled. */
            }

            try {
                auto identity = std::make_shared<protocol::Identity>( config::IdentityStore::ImportPem( path ) );

                ConfirmReplace( "Import " + path.filename().string() + " as your identity?", [this, identity] {
                    Replace( config::IdentityStore::Create( std::move( *identity ),
                                                            config::IdentityStore::DefaultSecurityLevel() ) );
                } );
            } catch ( const std::exception& exception ) {
                ShowStatus( std::string( "Could not import the identity: " ) + exception.what(), true );
            }
        } );
    }

    void IdentityPage::ExportIdentity() {
        Gtk::Window* parent = ParentWindow();

        if ( parent == nullptr ) {
            return;
        }

        const Glib::RefPtr<Gtk::FileDialog> dialog = Gtk::FileDialog::create();
        dialog->set_title( "Export identity" );
        dialog->set_initial_name( "ts-identity.pem" );
        dialog->save( *parent, [this, dialog]( const Glib::RefPtr<Gio::AsyncResult>& result ) {
            std::filesystem::path path;

            try {
                path = dialog->save_finish( result )->get_path();
            } catch ( const Glib::Error& ) {
                return; /* Cancelled. */
            }

            try {
                config::IdentityStore::ExportPem( path, config::IdentityStore::Load( m_IdentityPath ).identity );
                ShowStatus( "Identity exported to " + path.string() + ". Keep it private: it is your login.", false );
            } catch ( const std::exception& exception ) {
                ShowStatus( std::string( "Could not export the identity: " ) + exception.what(), true );
            }
        } );
    }

    void IdentityPage::IncreaseClicked() {
        if ( m_Search.joinable() ) {
            StopSearch();
            ShowStatus( "Stopped; the identity is unchanged.", false );
            return;
        }

        try {
            StartSearch();
        } catch ( const std::exception& exception ) {
            ShowStatus( std::string( "Could not raise the security level: " ) + exception.what(), true );
        }
    }

    void IdentityPage::StartSearch() {
        auto identity = std::make_unique<config::LocalIdentity>( config::IdentityStore::Load( m_IdentityPath ) );
        const auto targetLevel = static_cast<std::uint8_t>( m_Level.get_value_as_int() );

        if ( targetLevel <= identity->securityLevel ) {
            throw std::runtime_error( "the identity already has security level " + std::to_string( identity->securityLevel ) );
        }

        m_Searching = std::move( identity );
        m_SearchOffset = m_Searching->keyOffset;
        m_SearchDone = false;
        m_SearchFinished = false;
        m_SearchFailure = nullptr;
        m_Search = std::jthread( [this, targetLevel]( const std::stop_token& stop ) {
            try {
                m_SearchFinished = config::IdentityStore::IncreaseSecurityLevel( *m_Searching,
                                                                                 targetLevel,
                                                                                 stop,
                                                                                 [this]( std::uint64_t offset ) {
                                                                                     m_SearchOffset = offset;
                                                                                 } );
            } catch ( ... ) {
                m_SearchFailure = std::current_exception();
            }

            m_SearchDone = true;
        } );

        m_Status.set_visible( false );
        m_Progress.set_text( "Searching for security level " + std::to_string( targetLevel ) + "…" );
        SetSearching( true );
        m_SearchTimer = Glib::signal_timeout().connect( sigc::mem_fun( *this, &IdentityPage::PollSearch ), 200 );
    }

    bool IdentityPage::PollSearch() {
        if ( !m_SearchDone ) {
            m_Progress.set_text( "Searching for security level " + std::to_string( m_Level.get_value_as_int() ) +
                                 "… at key offset " + std::to_string( m_SearchOffset.load() ) );
            return true;
        }

        m_Search.join();
        SetSearching( false );

        try {
            if ( m_SearchFailure ) {
                std::rethrow_exception( m_SearchFailure );
            }

            if ( m_SearchFinished ) {
                config::IdentityStore::Save( m_IdentityPath, *m_Searching );
                Refresh();
                ShowStatus( "Security level raised to " + std::to_string( m_Searching->securityLevel ) + ".", false );
            }
        } catch ( const std::exception& exception ) {
            ShowStatus( std::string( "Could not raise the security level: " ) + exception.what(), true );
        }

        m_Searching.reset();
        return false;
    }

    void IdentityPage::StopSearch() {
        m_SearchTimer.disconnect();

        if ( !m_Search.joinable() ) {
            return;
        }

        m_Search.request_stop();
        m_Search.join();
        m_Searching.reset();
        SetSearching( false );
    }

    void IdentityPage::SetSearching( bool searching ) {
        m_Increase.set_label( searching ? "Stop" : "Increase" );
        m_Level.set_sensitive( !searching );
        m_Generate.set_sensitive( !searching );
        m_Import.set_sensitive( !searching );
        m_Progress.set_visible( searching );
    }

    void IdentityPage::ConfirmReplace( const std::string& message, std::function<void()> replace ) {
        Gtk::Window* parent = ParentWindow();

        if ( parent == nullptr ) {
            return;
        }

        const Glib::RefPtr<Gtk::AlertDialog> dialog = Gtk::AlertDialog::create( DisplayText( message ) );
        dialog->set_detail( "Your current identity is lost unless you exported it. Servers will treat you as a new user, "
                            "without the permissions and groups you had." );
        dialog->set_buttons( { "Cancel", "Replace" } );
        dialog->set_cancel_button( 0 );
        dialog->set_default_button( 0 );
        dialog->choose( *parent,
                        [this, dialog, replace = std::move( replace )]( const Glib::RefPtr<Gio::AsyncResult>& result ) {
                            try {
                                if ( dialog->choose_finish( result ) != 1 ) {
                                    return;
                                }
                            } catch ( const Glib::Error& ) {
                                return; /* Dismissed. */
                            }

                            try {
                                replace();
                            } catch ( const std::exception& exception ) {
                                ShowStatus( std::string( "Could not replace the identity: " ) + exception.what(), true );
                            }
                        } );
    }

    void IdentityPage::Replace( const config::LocalIdentity& identity ) {
        config::IdentityStore::Save( m_IdentityPath, identity );
        Refresh();
        ShowStatus( "Identity replaced.", false );
    }

    void IdentityPage::ShowStatus( const std::string& message, bool error ) {
        const Glib::ustring text = DisplayMarkup( message );
        m_Status.set_markup( error ? "<span foreground=\"#c01c28\">" + text + "</span>" : text );
        m_Status.set_visible( true );
    }

    Gtk::Window* IdentityPage::ParentWindow() {
        return dynamic_cast<Gtk::Window*>( get_root() );
    }

} // namespace ts::client::gtk
