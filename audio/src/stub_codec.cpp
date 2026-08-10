#include <algorithm>
#include <audio/codec.hpp>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>

namespace ts::audio {

    namespace {
        class StubEncoder final: public Encoder {
          public:
            std::size_t Encode( const PcmFrame& frame, std::span<std::byte> output ) override {
                const std::size_t bytes = std::min( output.size(), frame.samples.size() * sizeof( float ) );
                std::memcpy( output.data(), frame.samples.data(), bytes );
                return bytes;
            }
            void Reset() override {
            }
        };

        class StubDecoder final: public Decoder {
          public:
            std::size_t Decode( std::span<const std::byte> input,
                                DecodedAudio& output,
                                DecodeMode mode,
                                std::size_t recoverySamples ) override {
                output.samples.fill( 0.0F );
                if ( mode != DecodeMode::Normal ) {
                    output.sampleCount = std::min( recoverySamples, output.samples.size() );
                    return output.sampleCount;
                }
                if ( input.empty() ) {
                    output.sampleCount = 0;
                    return 0;
                }
                const std::size_t bytes = std::min( input.size(), output.samples.size() * sizeof( float ) );
                std::memcpy( output.samples.data(), input.data(), bytes );
                output.sampleCount = bytes / sizeof( float );
                return output.sampleCount;
            }
            void Reset() override {
            }
        };
    } // namespace

    std::unique_ptr<Encoder> CreateEncoder() {
        return std::make_unique<StubEncoder>();
    }
    std::unique_ptr<Decoder> CreateDecoder() {
        return std::make_unique<StubDecoder>();
    }

} // namespace ts::audio
