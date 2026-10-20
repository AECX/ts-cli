#ifndef TS_CLIENT_GTK_AUDIO_SETTINGS_WINDOW_HPP
#define TS_CLIENT_GTK_AUDIO_SETTINGS_WINDOW_HPP

#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <functional>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/levelbar.h>
#include <gtkmm/scale.h>
#include <gtkmm/stringlist.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>
#include <string>
#include <string_view>
#include <vector>

namespace ts::client::gtk {

    /*
     * Devices, RNNoise and the voice-activation threshold, with a live input
     * meter and a "test microphone" loopback. While the test runs the
     * microphone is muted for the server (restored when it stops or the
     * window is hidden), so testing never leaks into the channel.
     */
    class AudioSettingsWindow: public Gtk::Window {
      public:
        /* Called after a setting that belongs in config.conf changed. */
        using ChangedHandler = std::function<void()>;

        AudioSettingsWindow( audio::AudioEngine& audio, ChangedHandler onChanged );
        ~AudioSettingsWindow() override;

        AudioSettingsWindow( const AudioSettingsWindow& ) = delete;
        AudioSettingsWindow& operator=( const AudioSettingsWindow& ) = delete;

      protected:
        void on_show() override;
        void on_hide() override;

      private:
        /* A device drop-down: shows descriptions, selects by device name. */
        struct DeviceChoice {
            DeviceChoice();

            [[nodiscard]] std::string SelectedId() const;

            Glib::RefPtr<Gtk::StringList> descriptions;
            std::vector<std::string> ids;
            Gtk::DropDown dropDown;
        };

        void Populate();
        void PopulateDevices( DeviceChoice& choice, audio::DeviceKind kind, const std::string& selected );
        bool UpdateMeter();
        void Apply( std::string_view what, const std::function<void()>& change );
        void ShowError( const std::string& message );
        void SetMonitor( audio::CaptureMonitor monitor );

        audio::AudioEngine& m_Audio;
        ChangedHandler m_OnChanged;
        bool m_RestoreTransmit = false;
        bool m_Populating = false;
        sigc::connection m_MeterTimer;

        Gtk::Box m_Layout { Gtk::Orientation::VERTICAL, 10 };
        Gtk::Grid m_Grid;
        Gtk::Label m_InputLabel { "Microphone" };
        DeviceChoice m_Input;
        Gtk::Label m_OutputLabel { "Playback" };
        DeviceChoice m_Output;
        Gtk::Label m_NoiseLabel { "Noise suppression" };
        Gtk::CheckButton m_NoiseSuppression { "RNNoise" };
        Gtk::Label m_LevelLabel { "Input level" };
        Gtk::Box m_LevelBox { Gtk::Orientation::HORIZONTAL, 8 };
        Gtk::LevelBar m_Level;
        Gtk::Label m_VoiceIndicator;
        Gtk::Label m_ThresholdLabel { "Activation threshold" };
        Gtk::Scale m_Threshold { Gtk::Orientation::HORIZONTAL };
        Gtk::ToggleButton m_Test { "Test microphone" };
        Gtk::Label m_Notice;
        Gtk::Label m_Error;
        Gtk::Box m_Buttons { Gtk::Orientation::HORIZONTAL, 6 };
        Gtk::Button m_Close { "Close" };
    };

} // namespace ts::client::gtk

#endif // TS_CLIENT_GTK_AUDIO_SETTINGS_WINDOW_HPP
