#ifndef TS_PROTOCOL_VOICE_VOICE_CODEC_HPP
#define TS_PROTOCOL_VOICE_VOICE_CODEC_HPP
#include <cstddef>
#include <cstdint>
#include <protocol/packet/packet_flags.hpp>
#include <protocol/voice/voice.hpp>
#include <span>
#include <vector>

namespace ts::protocol {

    class VoiceWireCodec {
      public:
        [[nodiscard]] static std::vector<std::byte>
            EncodeClient( std::uint16_t voiceId, VoiceCodec codec, std::span<const std::byte> data );
        [[nodiscard]] static std::vector<std::byte> EncodeClientWhisper( std::uint16_t voiceId,
                                                                         VoiceCodec codec,
                                                                         std::span<const std::uint64_t> channelIds,
                                                                         std::span<const std::uint16_t> clientIds,
                                                                         std::span<const std::byte> data );
        [[nodiscard]] static std::vector<std::byte> EncodeClientGroupWhisper( std::uint16_t voiceId,
                                                                              VoiceCodec codec,
                                                                              GroupWhisperType type,
                                                                              GroupWhisperTarget target,
                                                                              std::uint64_t targetId,
                                                                              std::span<const std::byte> data );

        [[nodiscard]] static VoiceFrame DecodeServer( std::span<const std::byte> payload, PacketFlags flags, bool whisper );
    };
} // namespace ts::protocol
#endif // TS_PROTOCOL_VOICE_VOICE_CODEC_HPP
