#ifndef TS_PROTOCOL_VOICE_VOICE_HPP
#define TS_PROTOCOL_VOICE_VOICE_HPP
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ts::protocol {

    enum class VoiceCodec : std::uint8_t {
        SpeexNarrowband = 0,
        SpeexWideband = 1,
        SpeexUltraWideband = 2,
        CeltMono = 3,
        OpusVoice = 4,
        OpusMusic = 5
    };

    enum class GroupWhisperType : std::uint8_t { ServerGroup = 0, ChannelGroup = 1, ChannelCommander = 2, AllClients = 3 };

    enum class GroupWhisperTarget : std::uint8_t {
        AllChannels = 0,
        CurrentChannel = 1,
        ParentChannel = 2,
        AllParentChannel = 3,
        ChannelFamily = 4,
        CompleteChannelFamily = 5,
        Subchannels = 6
    };

    struct VoiceFrame {
        std::uint16_t voiceId = 0;
        std::uint16_t clientId = 0;
        VoiceCodec codec = VoiceCodec::OpusVoice;
        std::vector<std::byte> data;
        bool talkStart = false;
        bool talkEnd = false;
        bool whisper = false;
    };

} // namespace ts::protocol
#endif // TS_PROTOCOL_VOICE_VOICE_HPP
