#include <algorithm>
#include <array>
#include <audio/audio_types.hpp>
#include <audio/capture_filter.hpp>
#include <cstddef>
#include <memory>
#include <rnnoise.h>
#include <stdexcept>

namespace ts::audio {

    namespace {

        constexpr std::size_t RnnoiseFrameSamples = 480;
        constexpr float PcmScale = 32768.0F;

        static_assert( SampleRate == 48000 );
        static_assert( Channels == 1 );
        static_assert( SamplesPerFrame % RnnoiseFrameSamples == 0 );

        class RnnoiseCaptureFilter final: public CaptureFilter {
          public:
            RnnoiseCaptureFilter(): m_State( rnnoise_create( nullptr ), &rnnoise_destroy ) {
                if ( !m_State ) {
                    throw std::runtime_error( "Failed to create RNNoise state" );
                }
            }

            void Process( PcmFrame& frame ) override {
                std::array<float, RnnoiseFrameSamples> block {};

                for ( std::size_t offset = 0; offset < SamplesPerFrame; offset += RnnoiseFrameSamples ) {
                    for ( std::size_t index = 0; index < RnnoiseFrameSamples; ++index ) {
                        block[index] = frame.samples[offset + index] * PcmScale;
                    }

                    (void)rnnoise_process_frame( m_State.get(), block.data(), block.data() );

                    for ( std::size_t index = 0; index < RnnoiseFrameSamples; ++index ) {
                        frame.samples[offset + index] = std::clamp( block[index] / PcmScale, -1.0F, 1.0F );
                    }
                }
            }

          private:
            using State = std::unique_ptr<DenoiseState, void ( * )( DenoiseState* )>;
            State m_State;
        };

    } // namespace

    std::unique_ptr<CaptureFilter> CreateRnnoiseCaptureFilter() {
        return std::make_unique<RnnoiseCaptureFilter>();
    }

} // namespace ts::audio
