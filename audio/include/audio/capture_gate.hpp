#ifndef TS_AUDIO_CAPTURE_GATE_HPP
#define TS_AUDIO_CAPTURE_GATE_HPP

#include <audio/audio_types.hpp>
#include <cmath>
#include <cstddef>

namespace ts::audio {

    class CaptureGate {
      public:
        [[nodiscard]] bool Process( const PcmFrame& frame, float thresholdDb ) {
            double sumSquares = 0.0;

            for ( const float sample : frame.samples ) {
                const double value = static_cast<double>( sample );
                sumSquares += value * value;
            }

            const double meanSquare = sumSquares / static_cast<double>( frame.samples.size() );
            const double rms = std::sqrt( meanSquare );
            const double threshold = std::pow( 10.0, static_cast<double>( thresholdDb ) / 20.0 );

            if ( rms >= threshold ) {
                m_Active = true;
                m_HangoverRemaining = HangoverFrames;
                return true;
            }

            if ( !m_Active ) {
                return false;
            }

            if ( m_HangoverRemaining != 0 ) {
                --m_HangoverRemaining;
                return true;
            }

            m_Active = false;
            return false;
        }

        void Reset() {
            m_Active = false;
            m_HangoverRemaining = 0;
        }

        [[nodiscard]] bool Active() const {
            return m_Active;
        }

      private:
        static constexpr std::size_t HangoverFrames = 15;

        std::size_t m_HangoverRemaining = 0;
        bool m_Active = false;
    };

} // namespace ts::audio

#endif // TS_AUDIO_CAPTURE_GATE_HPP
