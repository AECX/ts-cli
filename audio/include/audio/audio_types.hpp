#ifndef TS_AUDIO_AUDIO_TYPES_HPP
#define TS_AUDIO_AUDIO_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ts::audio {

    constexpr std::uint32_t SampleRate = 48000;
    constexpr std::size_t Channels = 1;
    constexpr std::size_t FrameMilliseconds = 20;
    constexpr std::size_t SamplesPerFrame = SampleRate * FrameMilliseconds / 1000;
    constexpr std::size_t MaxOpusPacketMilliseconds = 120;
    constexpr std::size_t MaxDecodedSamples = SampleRate * MaxOpusPacketMilliseconds / 1000;
    constexpr std::size_t MaxEncodedBytes = 484;

    enum class DeviceKind { Input, Output };

    enum class NotificationType {
        Message,
        Success,
        Failure,
        Reminder,
    };

    struct AudioDevice {
        std::uint32_t id = 0;
        DeviceKind kind = DeviceKind::Input;
        std::string name;
        std::string description;
    };

    struct PcmFrame {
        std::array<float, SamplesPerFrame> samples {};
    };

    struct DecodedAudio {
        std::array<float, MaxDecodedSamples> samples {};
        std::size_t sampleCount = 0;
    };

    struct EncodedFrame {
        std::array<std::byte, MaxEncodedBytes> data {};
        std::size_t size = 0;
        std::uint64_t transmitRevision = 0;
        bool talkStart = false;
        bool talkEnd = false;
    };

    struct TransmitStateChange {
        bool enabled = false;
        std::uint64_t revision = 0;
    };

    struct IncomingEncodedFrame {
        std::uint16_t clientId = 0;
        std::uint16_t voiceId = 0;
        std::uint8_t codec = 0;
        std::array<std::byte, MaxEncodedBytes> data {};
        std::size_t size = 0;
        bool talkStart = false;
        bool talkEnd = false;
    };

    struct TalkerSettings {
        float volumeDb = 0.0F;
        bool muted = false;
    };

    struct AudioSettings {
        std::string input = "default";
        std::string output = "default";
        std::string captureFilter = "none";
        float activationThresholdDb = -45.0F;
    };

    struct AudioStatus {
        bool available = false;
        bool transmitEnabled = false;
        std::string input = "default";
        std::string output = "default";
        std::string captureFilter = "none";
        float activationThresholdDb = -45.0F;
        std::string error;
        std::uint64_t captureDrops = 0;
        std::uint64_t encodedDrops = 0;
        std::uint64_t receiveDrops = 0;
        std::uint64_t playbackUnderruns = 0;
    };

} // namespace ts::audio

#endif // TS_AUDIO_AUDIO_TYPES_HPP
