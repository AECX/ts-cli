#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <config/audio_settings.hpp>
#include <exception>
#include <functional>
#include <string>
#include <string_view>

namespace ts::config {

    void ApplyAudioSettings( audio::AudioEngine& engine,
                             const audio::AudioSettings& settings,
                             const AudioSettingsWarning& warn ) {
        const auto apply = [&warn]( std::string_view name, const std::function<void()>& action ) {
            try {
                action();
            } catch ( const std::exception& exception ) {
                if ( warn ) {
                    warn( "could not apply saved " + std::string( name ) + ": " + exception.what() );
                }
            }
        };

        apply( "activation threshold", [&engine, &settings] {
            engine.SetActivationThresholdDb( settings.activationThresholdDb );
        } );

        if ( !engine.Status().available ) {
            return;
        }

        apply( "input device", [&engine, &settings] {
            engine.SetInputDevice( settings.input );
        } );
        apply( "output device", [&engine, &settings] {
            engine.SetOutputDevice( settings.output );
        } );
        apply( "capture filter", [&engine, &settings] {
            engine.SetCaptureFilter( settings.captureFilter );
        } );
    }

} // namespace ts::config
