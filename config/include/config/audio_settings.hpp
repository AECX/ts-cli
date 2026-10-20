#ifndef TS_CONFIG_AUDIO_SETTINGS_HPP
#define TS_CONFIG_AUDIO_SETTINGS_HPP

#include <audio/audio_types.hpp>
#include <functional>
#include <string_view>

namespace ts::audio {
    class AudioEngine;
}

namespace ts::config {

    using AudioSettingsWarning = std::function<void( std::string_view message )>;

    /*
     * Applies persisted audio settings to a freshly started engine. A
     * setting this system can't honor right now (an unplugged device, a
     * filter missing from this build) is skipped and reported through `warn`
     * rather than failing startup or blocking the remaining settings. Device
     * and filter settings are skipped silently when the engine has no
     * backend at all; the engine reports that itself.
     */
    void ApplyAudioSettings( audio::AudioEngine& engine,
                             const audio::AudioSettings& settings,
                             const AudioSettingsWarning& warn );

} // namespace ts::config

#endif // TS_CONFIG_AUDIO_SETTINGS_HPP
