#include <audio/audio_backend.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ts::audio {

    namespace {
        class StubBackend final: public AudioBackend {
          public:
            void Start( CaptureCallback, PlaybackCallback ) override {
                throw std::runtime_error( "Stub audio backend has no device I/O" );
            }
            void Stop() override {
            }
            std::vector<AudioDevice> Devices() const override {
                return {};
            }
            void SetInputDevice( std::string_view ) override {
            }
            void SetOutputDevice( std::string_view ) override {
            }
        };
    } // namespace

    std::unique_ptr<AudioBackend> CreateAudioBackend() {
        return std::make_unique<StubBackend>();
    }

} // namespace ts::audio
