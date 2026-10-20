#include "test_support.hpp"

#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <config/audio_settings.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace ts::config::test {

    void RunAudioSettingsTests() {
        audio::AudioEngine engine;
        const bool available = engine.Status().available;
        std::vector<std::string> warnings;

        ApplyAudioSettings( engine,
                            audio::AudioSettings { .input = "ts-cli-test-no-such-device",
                                                   .output = "default",
                                                   .captureFilter = "none",
                                                   .activationThresholdDb = -30.0F },
                            [&warnings]( std::string_view message ) {
                                warnings.emplace_back( message );
                            } );

        Expect( engine.ActivationThresholdDb() == -30.0F, "Saved activation threshold was not applied" );

        if ( available ) {
            /* An unknown device is reported, and the remaining settings still apply. */
            Expect( warnings.size() == 1, "Unknown saved input device was not reported exactly once" );
            Expect( warnings.front().find( "input device" ) != std::string::npos, "Warning did not name the failing setting" );
            ExpectEqual( engine.Status().captureFilter, std::string( "none" ), "Later settings were skipped" );
        } else {
            Expect( warnings.empty(), "Device settings were reported although the engine has no backend" );
        }
    }

} // namespace ts::config::test
