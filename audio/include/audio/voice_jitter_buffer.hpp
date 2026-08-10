#ifndef TS_AUDIO_VOICE_JITTER_BUFFER_HPP
#define TS_AUDIO_VOICE_JITTER_BUFFER_HPP

#include <audio/audio_types.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

namespace ts::audio {

    enum class JitterPullKind { Wait, Frame, PacketLoss, End };

    struct JitterPullResult {
        JitterPullKind kind = JitterPullKind::Wait;
        IncomingEncodedFrame frame;
        bool streamStart = false;
        bool hasRecoveryFrame = false;
    };

    class VoiceJitterBuffer {
      public:
        void Push( const IncomingEncodedFrame& frame );
        [[nodiscard]] JitterPullResult Pull();
        void Reset();

        [[nodiscard]] std::size_t Pending() const;

      private:
        [[nodiscard]] static std::uint16_t Distance( std::uint16_t from, std::uint16_t to );
        [[nodiscard]] bool HasFutureData() const;
        void Advance();

        static constexpr std::size_t PrebufferFrames = 3;
        static constexpr std::size_t MaxPendingFrames = 32;
        static constexpr std::uint16_t MaxReorderDistance = 64;
        static constexpr std::size_t MaxConsecutiveLosses = 3;

        std::optional<std::uint16_t> m_ExpectedVoiceId;
        std::optional<std::uint16_t> m_EndVoiceId;
        std::map<std::uint16_t, IncomingEncodedFrame> m_Pending;
        std::size_t m_ConsecutiveLosses = 0;
        bool m_Started = false;
        bool m_FirstOutput = false;
    };

} // namespace ts::audio

#endif // TS_AUDIO_VOICE_JITTER_BUFFER_HPP
