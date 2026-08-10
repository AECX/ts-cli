#include <algorithm>
#include <audio/notification_synthesizer.hpp>
#include <cmath>

namespace ts::audio {

    void NotificationSynthesizer::Start( NotificationType type ) {

        m_Pattern = PatternFor( type );
        m_StepIndex = 0;
        m_StepSample = 0;
    }

    bool NotificationSynthesizer::Mix( PcmFrame& frame ) {

        AdvanceCompletedSteps();

        if ( !Active() ) {

            return false;
        }

        bool produced = false;

        for ( std::size_t outputIndex = 0; outputIndex < frame.samples.size(); ) {

            AdvanceCompletedSteps();

            if ( !Active() ) {

                break;
            }

            const Step& step = m_Pattern.steps[m_StepIndex];
            const std::size_t stepSamples = SamplesFor( step );

            /*
             *          * A silence step is still part of the notification timeline.
             *                   * Consume output samples so the pause between notes is preserved.
             *                            */
            produced = true;

            if ( step.frequencyHz > 0.0F ) {

                const float time = static_cast<float>( m_StepSample ) / static_cast<float>( SampleRate );

                const float progress =
                    stepSamples > 1 ? static_cast<float>( m_StepSample ) / static_cast<float>( stepSamples - 1 ) : 1.0F;

                constexpr float TwoPi = 6.28318530717958647692F;

                frame.samples[outputIndex] += std::sin( TwoPi * step.frequencyHz * time ) * step.volume * Envelope( progress );
            }

            ++outputIndex;
            ++m_StepSample;
        }

        AdvanceCompletedSteps();

        return produced;
    }

    bool NotificationSynthesizer::Active() const {

        return m_StepIndex < m_Pattern.stepCount;
    }

    std::size_t NotificationSynthesizer::SamplesFor( const Step& step ) {

        return static_cast<std::size_t>( SampleRate ) * step.durationMilliseconds / 1000;
    }

    float NotificationSynthesizer::Envelope( float progress ) {

        constexpr float Attack = 0.08F;
        constexpr float Release = 0.25F;

        if ( progress < Attack ) {

            return progress / Attack;
        }

        if ( progress > 1.0F - Release ) {

            return ( 1.0F - progress ) / Release;
        }

        return 1.0F;
    }

    void NotificationSynthesizer::AdvanceCompletedSteps() {

        while ( Active() ) {

            const Step& step = m_Pattern.steps[m_StepIndex];

            if ( m_StepSample < SamplesFor( step ) ) {

                return;
            }

            ++m_StepIndex;
            m_StepSample = 0;
        }
    }

    NotificationSynthesizer::Pattern NotificationSynthesizer::PatternFor( NotificationType type ) {

        Pattern pattern;

        switch ( type ) {

            case NotificationType::Message:

                pattern.steps[0] = {
                    .frequencyHz = 880.0F,
                    .durationMilliseconds = 120,
                    .volume = 0.18F,
                };

                pattern.stepCount = 1;
                break;

            case NotificationType::Success:

                pattern.steps[0] = {
                    .frequencyHz = 523.25F,
                    .durationMilliseconds = 80,
                    .volume = 0.17F,
                };

                pattern.steps[1] = {
                    .frequencyHz = 0.0F,
                    .durationMilliseconds = 20,
                    .volume = 0.0F,
                };

                pattern.steps[2] = {
                    .frequencyHz = 659.25F,
                    .durationMilliseconds = 130,
                    .volume = 0.19F,
                };

                pattern.stepCount = 3;
                break;

            case NotificationType::Failure:

                pattern.steps[0] = {
                    .frequencyHz = 520.0F,
                    .durationMilliseconds = 100,
                    .volume = 0.17F,
                };

                pattern.steps[1] = {
                    .frequencyHz = 0.0F,
                    .durationMilliseconds = 20,
                    .volume = 0.0F,
                };

                pattern.steps[2] = {
                    .frequencyHz = 330.0F,
                    .durationMilliseconds = 170,
                    .volume = 0.19F,
                };

                pattern.stepCount = 3;
                break;

            case NotificationType::Reminder:

                pattern.steps[0] = {
                    .frequencyHz = 783.99F,
                    .durationMilliseconds = 70,
                    .volume = 0.16F,
                };

                pattern.steps[1] = {
                    .frequencyHz = 0.0F,
                    .durationMilliseconds = 80,
                    .volume = 0.0F,
                };

                pattern.steps[2] = {
                    .frequencyHz = 1046.50F,
                    .durationMilliseconds = 100,
                    .volume = 0.18F,
                };

                pattern.stepCount = 3;
                break;
        }

        return pattern;
    }

} // namespace ts::audio
