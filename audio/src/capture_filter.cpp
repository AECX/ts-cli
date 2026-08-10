#include <audio/capture_filter.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ts::audio {

#ifdef TS_AUDIO_HAVE_RNNOISE
    [[nodiscard]] std::unique_ptr<CaptureFilter> CreateRnnoiseCaptureFilter();
#endif

    namespace {

        class PassthroughCaptureFilter final: public CaptureFilter {
          public:
            void Process( PcmFrame& ) override {
            }
        };

    } // namespace

    std::unique_ptr<CaptureFilter> CreateCaptureFilter( std::string_view name ) {
        if ( name == "none" ) {
            return std::make_unique<PassthroughCaptureFilter>();
        }

#ifdef TS_AUDIO_HAVE_RNNOISE
        if ( name == "rnnoise" ) {
            return CreateRnnoiseCaptureFilter();
        }
#endif

        throw std::runtime_error( "Audio capture filter is unavailable: " + std::string( name ) );
    }

    std::vector<std::string> AvailableCaptureFilters() {
        std::vector<std::string> filters { "none" };

#ifdef TS_AUDIO_HAVE_RNNOISE
        filters.emplace_back( "rnnoise" );
#endif

        return filters;
    }

} // namespace ts::audio
