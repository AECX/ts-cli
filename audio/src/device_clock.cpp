#include <audio/audio_types.hpp>
#include <iomanip>
#include <sstream>
#include <string>

namespace ts::audio {

    std::string FormatDeviceClock( const DeviceClock& clock ) {
        if ( clock.rate == 0 ) {
            return "unknown";
        }

        std::ostringstream text;
        text << clock.rate << " Hz";

        if ( clock.period != 0 ) {
            const double milliseconds = 1000.0 * static_cast<double>( clock.period ) / static_cast<double>( clock.rate );
            text << ", " << clock.period << "-sample periods (" << std::fixed << std::setprecision( 1 ) << milliseconds
                 << " ms)";
        }

        if ( clock.rate != SampleRate ) {
            text << ", resampled";
        }

        return text.str();
    }

} // namespace ts::audio
