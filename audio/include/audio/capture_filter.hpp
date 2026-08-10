#ifndef TS_AUDIO_CAPTURE_FILTER_HPP
#define TS_AUDIO_CAPTURE_FILTER_HPP

#include <audio/audio_types.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ts::audio {

    class CaptureFilter {
      public:
        virtual ~CaptureFilter() = default;

        CaptureFilter( const CaptureFilter& ) = delete;
        CaptureFilter& operator=( const CaptureFilter& ) = delete;
        CaptureFilter( CaptureFilter&& ) = delete;
        CaptureFilter& operator=( CaptureFilter&& ) = delete;

        virtual void Process( PcmFrame& frame ) = 0;

      protected:
        CaptureFilter() = default;
    };

    [[nodiscard]] std::unique_ptr<CaptureFilter> CreateCaptureFilter( std::string_view name );
    [[nodiscard]] std::vector<std::string> AvailableCaptureFilters();

} // namespace ts::audio

#endif // TS_AUDIO_CAPTURE_FILTER_HPP
