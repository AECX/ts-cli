#ifndef TS_AUDIO_AUDIO_BACKEND_HPP
#define TS_AUDIO_AUDIO_BACKEND_HPP

#include <audio/audio_types.hpp>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace ts::audio {

    class AudioBackend {
      public:
        using CaptureCallback = std::function<void( std::span<const float> )>;
        using PlaybackCallback = std::function<void( std::span<float> )>;

        virtual ~AudioBackend() = default;

        virtual void Start( CaptureCallback capture, PlaybackCallback playback ) = 0;
        virtual void Stop() = 0;

        [[nodiscard]] virtual std::vector<AudioDevice> Devices() const = 0;
        virtual void SetInputDevice( std::string_view selector ) = 0;
        virtual void SetOutputDevice( std::string_view selector ) = 0;
    };

    [[nodiscard]] std::unique_ptr<AudioBackend> CreateAudioBackend();

} // namespace ts::audio

#endif // TS_AUDIO_AUDIO_BACKEND_HPP
