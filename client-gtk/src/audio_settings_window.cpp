#include <algorithm>
#include <audio/audio_types.hpp>
#include <client/gtk/audio_settings_window.hpp>
#include <client/gtk/display_text.hpp>
#include <exception>
#include <functional>
#include <glibmm/main.h>
#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ts::client::gtk {

    namespace {

        constexpr unsigned int MeterMilliseconds = 50;
        constexpr double MinimumDb = -100.0;

        [[nodiscard]] Glib::ustring FormatDb( double value ) {
            std::ostringstream text;
            text << std::fixed << std::setprecision( 0 ) << value << " dBFS";
            return text.str();
        }

    } // namespace

    AudioSettingsWindow::DeviceChoice::DeviceChoice(): descriptions( Gtk::StringList::create() ) {
        dropDown.set_model( descriptions );
        dropDown.set_hexpand( true );
    }

    std::string AudioSettingsWindow::DeviceChoice::SelectedId() const {
        const guint selected = dropDown.get_selected();
        return selected < ids.size() ? ids[selected] : std::string {};
    }

    AudioSettingsWindow::AudioSettingsWindow( audio::AudioEngine& audio, ChangedHandler onChanged ):
        m_Audio( audio ), m_OnChanged( std::move( onChanged ) ) {
        set_title( "Audio settings" );
        set_default_size( 460, -1 );
        set_resizable( false );
        set_hide_on_close( true );

        for ( Gtk::Label* label : { &m_InputLabel, &m_OutputLabel, &m_NoiseLabel, &m_LevelLabel, &m_ThresholdLabel } ) {
            label->set_xalign( 0.0F );
        }

        /* Plain bar: the default low/high/full offsets assume a 0..1 "fill" meaning, not a level. */
        m_Level.set_min_value( 0.0 );
        m_Level.set_max_value( 1.0 );
        m_Level.remove_offset_value( GTK_LEVEL_BAR_OFFSET_LOW );
        m_Level.remove_offset_value( GTK_LEVEL_BAR_OFFSET_HIGH );
        m_Level.remove_offset_value( GTK_LEVEL_BAR_OFFSET_FULL );
        m_Level.set_hexpand( true );
        m_Level.set_valign( Gtk::Align::CENTER );
        m_Level.set_size_request( -1, 10 );
        m_VoiceIndicator.set_width_chars( 8 );
        m_LevelBox.append( m_Level );
        m_LevelBox.append( m_VoiceIndicator );

        m_Threshold.set_range( MinimumDb, 0.0 );
        m_Threshold.set_increments( 1.0, 5.0 );
        m_Threshold.set_digits( 0 );
        m_Threshold.set_draw_value( true );
        m_Threshold.set_value_pos( Gtk::PositionType::RIGHT );
        m_Threshold.set_hexpand( true );
        m_Threshold.set_format_value_func( []( double value ) {
            return FormatDb( value );
        } );

        m_Grid.set_row_spacing( 8 );
        m_Grid.set_column_spacing( 12 );
        m_Grid.attach( m_InputLabel, 0, 0 );
        m_Grid.attach( m_Input.dropDown, 1, 0 );
        m_Grid.attach( m_OutputLabel, 0, 1 );
        m_Grid.attach( m_Output.dropDown, 1, 1 );
        m_Grid.attach( m_NoiseLabel, 0, 2 );
        m_Grid.attach( m_NoiseSuppression, 1, 2 );
        m_Grid.attach( m_LevelLabel, 0, 3 );
        m_Grid.attach( m_LevelBox, 1, 3 );
        m_Grid.attach( m_ThresholdLabel, 0, 4 );
        m_Grid.attach( m_Threshold, 1, 4 );

        m_Test.set_tooltip_text( "Play back exactly what would be transmitted: after noise suppression, "
                                 "voice activation and Opus encoding" );

        m_Notice.set_markup( "<small><i>Your microphone is muted on the server while this window is open.</i></small>" );
        m_Notice.set_xalign( 0.0F );
        m_Notice.set_wrap( true );

        m_Error.set_xalign( 0.0F );
        m_Error.set_wrap( true );
        m_Error.set_visible( false );

        m_Close.set_hexpand( true );
        m_Close.set_halign( Gtk::Align::END );
        m_Buttons.append( m_Test );
        m_Buttons.append( m_Close );

        m_Layout.set_margin( 12 );
        m_Layout.append( m_Grid );
        m_Layout.append( m_Notice );
        m_Layout.append( m_Error );
        m_Layout.append( m_Buttons );
        set_child( m_Layout );

        m_Input.dropDown.property_selected().signal_changed().connect( [this] {
            Apply( "microphone", [this] {
                m_Audio.SetInputDevice( m_Input.SelectedId() );
            } );
        } );
        m_Output.dropDown.property_selected().signal_changed().connect( [this] {
            Apply( "playback device", [this] {
                m_Audio.SetOutputDevice( m_Output.SelectedId() );
            } );
        } );
        m_NoiseSuppression.signal_toggled().connect( [this] {
            Apply( "noise suppression", [this] {
                m_Audio.SetCaptureFilter( m_NoiseSuppression.get_active() ? "rnnoise" : "none" );
            } );
        } );
        m_Threshold.signal_value_changed().connect( [this] {
            Apply( "activation threshold", [this] {
                m_Audio.SetActivationThresholdDb( static_cast<float>( m_Threshold.get_value() ) );
            } );
        } );
        m_Test.signal_toggled().connect( [this] {
            if ( !m_Populating ) {
                SetMonitor( m_Test.get_active() ? audio::CaptureMonitor::Loopback : audio::CaptureMonitor::Level );
            }
        } );
        m_Close.signal_clicked().connect( [this] {
            set_visible( false );
        } );
    }

    AudioSettingsWindow::~AudioSettingsWindow() {
        m_MeterTimer.disconnect();
    }

    void AudioSettingsWindow::on_show() {
        m_Error.set_visible( false );
        m_RestoreTransmit = m_Audio.TransmitEnabled();

        if ( m_RestoreTransmit ) {
            m_Audio.SetTransmitEnabled( false );
        }

        Populate();
        SetMonitor( audio::CaptureMonitor::Level );
        m_MeterTimer =
            Glib::signal_timeout().connect( sigc::mem_fun( *this, &AudioSettingsWindow::UpdateMeter ), MeterMilliseconds );

        Gtk::Window::on_show();
    }

    void AudioSettingsWindow::on_hide() {
        m_MeterTimer.disconnect();

        m_Populating = true;
        m_Test.set_active( false );
        m_Populating = false;
        m_Audio.SetCaptureMonitor( audio::CaptureMonitor::Off );

        if ( m_RestoreTransmit ) {
            m_RestoreTransmit = false;

            try {
                m_Audio.SetTransmitEnabled( true );
            } catch ( const std::exception& ) {
                /* The audio backend went away while the window was open; stay muted. */
            }
        }

        Gtk::Window::on_hide();
    }

    void AudioSettingsWindow::Populate() {
        m_Populating = true;

        const audio::AudioStatus status = m_Audio.Status();
        const std::vector<std::string> filters = m_Audio.CaptureFilters();
        const bool hasNoiseSuppression = std::find( filters.begin(), filters.end(), "rnnoise" ) != filters.end();

        PopulateDevices( m_Input, audio::DeviceKind::Input, status.input );
        PopulateDevices( m_Output, audio::DeviceKind::Output, status.output );

        m_NoiseSuppression.set_active( status.captureFilter == "rnnoise" );
        m_NoiseSuppression.set_sensitive( status.available && hasNoiseSuppression );
        m_NoiseSuppression.set_tooltip_text( hasNoiseSuppression ? "RNNoise microphone noise suppression"
                                                                 : "This build has no RNNoise support" );
        m_Threshold.set_value( static_cast<double>( status.activationThresholdDb ) );

        for ( Gtk::Widget* control :
              std::initializer_list<Gtk::Widget*> { &m_Input.dropDown, &m_Output.dropDown, &m_Threshold, &m_Test } ) {
            control->set_sensitive( status.available );
        }

        if ( !status.available ) {
            ShowError( "Audio is unavailable" + ( status.error.empty() ? std::string {} : ": " + status.error ) );
        }

        m_Populating = false;
    }

    void AudioSettingsWindow::PopulateDevices( DeviceChoice& choice, audio::DeviceKind kind, const std::string& selected ) {
        std::vector<Glib::ustring> descriptions { "System default" };
        choice.ids = { "default" };

        try {
            for ( const audio::AudioDevice& device : m_Audio.Devices() ) {
                if ( device.kind == kind ) {
                    descriptions.push_back( DisplayText( device.description ) );
                    choice.ids.push_back( device.name );
                }
            }
        } catch ( const std::exception& ) {
            /* Unavailable audio is reported once by Populate(). */
        }

        auto active = std::find( choice.ids.begin(), choice.ids.end(), selected );

        if ( active == choice.ids.end() ) {
            /* A selector saved as an ID or partial name; keep showing it as configured. */
            descriptions.push_back( DisplayText( selected ) );
            choice.ids.push_back( selected );
            active = std::prev( choice.ids.end() );
        }

        choice.descriptions->splice( 0, choice.descriptions->get_n_items(), descriptions );
        choice.dropDown.set_selected( static_cast<guint>( active - choice.ids.begin() ) );
    }

    bool AudioSettingsWindow::UpdateMeter() {
        const audio::InputLevel level = m_Audio.CurrentInputLevel();
        const double fraction = ( static_cast<double>( level.levelDb ) - MinimumDb ) / -MinimumDb;

        m_Level.set_value( std::clamp( fraction, 0.0, 1.0 ) );
        m_VoiceIndicator.set_markup( level.voiceActive ? "<span foreground=\"#2ec27e\">● voice</span>"
                                                       : "<span alpha=\"50%\">○ silent</span>" );
        return true;
    }

    void AudioSettingsWindow::Apply( std::string_view what, const std::function<void()>& change ) {
        if ( m_Populating ) {
            return;
        }

        try {
            change();
            m_Error.set_visible( false );

            if ( m_OnChanged ) {
                m_OnChanged();
            }
        } catch ( const std::exception& exception ) {
            ShowError( "Could not change " + std::string( what ) + ": " + exception.what() );
        }
    }

    void AudioSettingsWindow::ShowError( const std::string& message ) {
        m_Error.set_markup( "<span foreground=\"#c01c28\">" + DisplayMarkup( message ) + "</span>" );
        m_Error.set_visible( true );
    }

    void AudioSettingsWindow::SetMonitor( audio::CaptureMonitor monitor ) {
        try {
            m_Audio.SetCaptureMonitor( monitor );
        } catch ( const std::exception& ) {
            /* Without a backend there is nothing to meter; Populate() already said why. */
        }
    }

} // namespace ts::client::gtk
