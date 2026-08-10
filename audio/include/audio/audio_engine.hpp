#ifndef TS_AUDIO_AUDIO_ENGINE_HPP
#define TS_AUDIO_AUDIO_ENGINE_HPP

#include <array>
#include <atomic>
#include <audio/audio_backend.hpp>
#include <audio/audio_types.hpp>
#include <audio/capture_filter.hpp>
#include <audio/capture_gate.hpp>
#include <audio/codec.hpp>
#include <audio/notification_synthesizer.hpp>
#include <audio/spsc_ring.hpp>
#include <audio/voice_jitter_buffer.hpp>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace ts::audio {

    class AudioEngine {
      public:
        using NetworkWakeCallback = std::function<void()>;

        explicit AudioEngine( NetworkWakeCallback networkWake = {} );
        ~AudioEngine();

        AudioEngine( const AudioEngine& ) = delete;
        AudioEngine& operator=( const AudioEngine& ) = delete;
        AudioEngine( AudioEngine&& ) = delete;
        AudioEngine& operator=( AudioEngine&& ) = delete;

        [[nodiscard]] std::vector<AudioDevice> Devices() const;
        void SetInputDevice( std::string_view selector );
        void SetOutputDevice( std::string_view selector );
        void SetCaptureFilter( std::string_view name );
        [[nodiscard]] std::vector<std::string> CaptureFilters() const;
        void SetActivationThresholdDb( float thresholdDb );
        [[nodiscard]] float ActivationThresholdDb() const;
        [[nodiscard]] AudioSettings Settings() const;

        void SetTransmitEnabled( bool enabled );
        [[nodiscard]] bool TransmitEnabled() const;
        [[nodiscard]] std::optional<TransmitStateChange> TakeTransmitStateChange();

        [[nodiscard]] AudioStatus Status() const;
        void Notify( NotificationType type );

        [[nodiscard]] bool TryPopOutgoing( EncodedFrame& frame );
        bool SubmitIncoming( const IncomingEncodedFrame& frame );
        void SetTalkerSettings( std::uint16_t clientId, const TalkerSettings& settings );
        void RemoveTalker( std::uint16_t clientId );

      private:
        struct TalkerState {
            std::unique_ptr<Decoder> decoder;
            VoiceJitterBuffer jitter;
            std::deque<float> decodedSamples;
            std::size_t lastPacketSamples = SamplesPerFrame;
        };

        struct TalkerMixSettings {
            float gain = 1.0F;
            bool muted = false;
        };

        void OnCapturedSamples( std::span<const float> samples );
        void RenderPlayback( std::span<float> output );
        void Worker( std::stop_token stopToken );
        void ProcessCapture( Encoder& encoder,
                             TransmitStateChange& workerState,
                             CaptureGate& gate,
                             std::size_t& talkStartRemaining );
        void QueueTalkEnd( std::uint64_t transmitRevision );
        [[nodiscard]] TransmitStateChange LoadTransmitState() const;
        void ProcessIncoming();
        void MixPlayback();
        void WakeWorker();
        void NotifyNetwork() const;
        void RequireAvailable() const;

        static constexpr std::size_t CaptureQueueCapacity = 16;
        static constexpr std::size_t OutgoingQueueCapacity = 16;
        static constexpr std::size_t IncomingQueueCapacity = 128;
        static constexpr std::size_t PlaybackQueueCapacity = 32;
        static constexpr std::size_t TalkStartPackets = 5;
        static constexpr std::uint8_t NoNotificationRequested = 0xFF;

        std::unique_ptr<AudioBackend> m_Backend;
        NetworkWakeCallback m_NetworkWake;
        std::jthread m_Worker;

        SpscRing<PcmFrame, CaptureQueueCapacity> m_CaptureQueue;
        SpscRing<EncodedFrame, OutgoingQueueCapacity> m_OutgoingQueue;
        SpscRing<IncomingEncodedFrame, IncomingQueueCapacity> m_IncomingQueue;
        SpscRing<PcmFrame, PlaybackQueueCapacity> m_PlaybackQueue;

        PcmFrame m_CaptureAssembly;
        std::size_t m_CaptureAssemblyOffset = 0;

        PcmFrame m_PlaybackCurrent;
        std::size_t m_PlaybackOffset = SamplesPerFrame;

        std::map<std::uint16_t, TalkerState> m_Talkers;
        std::map<std::uint16_t, TalkerMixSettings> m_TalkerMixSettings;
        SpscRing<std::uint16_t, IncomingQueueCapacity> m_TalkerResetQueue;

        std::atomic<std::uint8_t> m_NotificationRequested { NoNotificationRequested };
        std::atomic<std::uint64_t> m_TransmitState { 2 };
        std::atomic<float> m_ActivationThresholdDb { -45.0F };
        std::uint64_t m_NetworkTransmitRevision = 0;

        std::atomic<std::uint64_t> m_WorkSignal { 0 };
        std::condition_variable m_WorkCondition;
        std::mutex m_WorkMutex;
        std::atomic<std::uint64_t> m_CaptureDrops { 0 };
        std::atomic<std::uint64_t> m_EncodedDrops { 0 };
        std::atomic<std::uint64_t> m_ReceiveDrops { 0 };
        std::atomic<std::uint64_t> m_PlaybackUnderruns { 0 };

        mutable std::mutex m_StateMutex;
        mutable std::mutex m_FilterMutex;
        mutable std::mutex m_TalkerSettingsMutex;
        NotificationSynthesizer m_NotificationSynthesizer;
        std::unique_ptr<CaptureFilter> m_CaptureFilter;
        std::string m_CaptureFilterName = "none";
        bool m_Available = false;
        std::string m_InputSelector = "default";
        std::string m_OutputSelector = "default";
        std::string m_Error;
    };

} // namespace ts::audio

#endif // TS_AUDIO_AUDIO_ENGINE_HPP
