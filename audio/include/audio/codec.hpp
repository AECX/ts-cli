#ifndef TS_AUDIO_CODEC_HPP
#define TS_AUDIO_CODEC_HPP

#include <audio/audio_types.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace ts::audio {

    class Encoder {
      public:
        virtual ~Encoder() = default;
        [[nodiscard]] virtual std::size_t Encode( const PcmFrame& frame, std::span<std::byte> output ) = 0;
        virtual void Reset() = 0;
    };

    enum class DecodeMode { Normal, PacketLoss, ForwardErrorCorrection };

    class Decoder {
      public:
        virtual ~Decoder() = default;
        [[nodiscard]] virtual std::size_t
            Decode( std::span<const std::byte> input, DecodedAudio& output, DecodeMode mode, std::size_t recoverySamples ) = 0;
        virtual void Reset() = 0;
    };

    [[nodiscard]] std::unique_ptr<Encoder> CreateEncoder();
    [[nodiscard]] std::unique_ptr<Decoder> CreateDecoder();

} // namespace ts::audio

#endif // TS_AUDIO_CODEC_HPP
