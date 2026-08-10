#ifndef TS_AUDIO_NOTIFICATION_SYNTHESIZER_HPP
#define TS_AUDIO_NOTIFICATION_SYNTHESIZER_HPP

#include <array>
#include <audio/audio_types.hpp>
#include <cstddef>
#include <cstdint>

namespace ts::audio {

    class NotificationSynthesizer {

      public:
        void Start( NotificationType type );

        [[nodiscard]] bool Mix( PcmFrame& frame );
        [[nodiscard]] bool Active() const;

      private:
        static constexpr std::size_t MaxSteps = 3;
        struct Step {
            float frequencyHz = 0.0F;

            std::uint16_t durationMilliseconds = 0;

            float volume = 0.0F;
        };

        struct Pattern {
            std::array<Step, MaxSteps> steps {};

            std::size_t stepCount = 0;
        };

        [[nodiscard]] static Pattern PatternFor( NotificationType type );
        [[nodiscard]] static std::size_t SamplesFor( const Step& step );
        [[nodiscard]] static float Envelope( float progress );

        void AdvanceCompletedSteps();
        Pattern m_Pattern;
        std::size_t m_StepIndex = 0;
        std::size_t m_StepSample = 0;
    };

} // namespace ts::audio
#endif // TS_AUDIO_NOTIFICATION_SYNTHESIZER_HPP
