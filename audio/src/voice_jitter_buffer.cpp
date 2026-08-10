#include <audio/voice_jitter_buffer.hpp>
#include <cstddef>
#include <cstdint>

namespace ts::audio {

    void VoiceJitterBuffer::Push( const IncomingEncodedFrame& frame ) {
        if ( frame.talkEnd ) {
            if ( !m_ExpectedVoiceId ) {
                m_ExpectedVoiceId = frame.voiceId;
                m_FirstOutput = true;
            }
            const std::uint16_t distance = Distance( *m_ExpectedVoiceId, frame.voiceId );
            if ( distance < 0x8000U ) {
                m_EndVoiceId = frame.voiceId;
            }
            return;
        }

        if ( !m_ExpectedVoiceId ) {
            m_ExpectedVoiceId = frame.voiceId;
            m_FirstOutput = true;
        } else {
            const std::uint16_t forward = Distance( *m_ExpectedVoiceId, frame.voiceId );

            if ( forward >= 0x8000U ) {
                const std::uint16_t backward = Distance( frame.voiceId, *m_ExpectedVoiceId );
                if ( !m_Started && frame.talkStart && backward <= MaxReorderDistance ) {
                    m_ExpectedVoiceId = frame.voiceId;
                } else {
                    return;
                }
            } else if ( forward > MaxReorderDistance ) {
                Reset();
                m_ExpectedVoiceId = frame.voiceId;
                m_FirstOutput = true;
            }
        }

        m_Pending.try_emplace( frame.voiceId, frame );

        while ( m_Pending.size() > MaxPendingFrames ) {
            auto furthest = m_Pending.begin();
            std::uint16_t furthestDistance = 0;
            for ( auto entry = m_Pending.begin(); entry != m_Pending.end(); ++entry ) {
                const std::uint16_t distance = Distance( *m_ExpectedVoiceId, entry->first );
                if ( distance < 0x8000U && distance >= furthestDistance ) {
                    furthest = entry;
                    furthestDistance = distance;
                }
            }
            m_Pending.erase( furthest );
        }
    }

    JitterPullResult VoiceJitterBuffer::Pull() {
        if ( !m_ExpectedVoiceId ) {
            return {};
        }

        if ( !m_Started ) {
            if ( m_EndVoiceId && *m_EndVoiceId == *m_ExpectedVoiceId ) {
                Reset();
                return JitterPullResult { .kind = JitterPullKind::End, .frame = {}, .streamStart = false };
            }

            if ( m_Pending.size() < PrebufferFrames && !m_EndVoiceId ) {
                return {};
            }

            m_Started = true;
        }

        if ( m_EndVoiceId && *m_EndVoiceId == *m_ExpectedVoiceId ) {
            Reset();
            return JitterPullResult { .kind = JitterPullKind::End, .frame = {}, .streamStart = false };
        }

        const auto ready = m_Pending.find( *m_ExpectedVoiceId );
        if ( ready != m_Pending.end() ) {
            JitterPullResult result { .kind = JitterPullKind::Frame, .frame = ready->second, .streamStart = m_FirstOutput };
            m_Pending.erase( ready );
            m_ConsecutiveLosses = 0;
            m_FirstOutput = false;
            Advance();
            return result;
        }

        if ( !HasFutureData() ) {
            return {};
        }

        if ( m_ConsecutiveLosses < MaxConsecutiveLosses ) {
            JitterPullResult result { .kind = JitterPullKind::PacketLoss, .frame = {}, .streamStart = false };
            const std::uint16_t recoveryVoiceId = static_cast<std::uint16_t>( *m_ExpectedVoiceId + 1U );
            const auto recovery = m_Pending.find( recoveryVoiceId );
            if ( recovery != m_Pending.end() ) {
                /*
                 * Keep the next packet queued for normal playback, but expose
                 * it here so an Opus decoder can recover the missing packet
                 * from in-band FEC when the sender included it.
                 */
                result.frame = recovery->second;
                result.hasRecoveryFrame = true;
            }

            ++m_ConsecutiveLosses;
            m_FirstOutput = false;
            Advance();
            return result;
        }

        Reset();
        return JitterPullResult { .kind = JitterPullKind::End, .frame = {}, .streamStart = false };
    }

    void VoiceJitterBuffer::Reset() {
        m_ExpectedVoiceId.reset();
        m_EndVoiceId.reset();
        m_Pending.clear();
        m_ConsecutiveLosses = 0;
        m_Started = false;
        m_FirstOutput = false;
    }

    std::size_t VoiceJitterBuffer::Pending() const {
        return m_Pending.size();
    }

    std::uint16_t VoiceJitterBuffer::Distance( std::uint16_t from, std::uint16_t to ) {
        return static_cast<std::uint16_t>( to - from );
    }

    bool VoiceJitterBuffer::HasFutureData() const {
        if ( !m_ExpectedVoiceId ) {
            return false;
        }
        if ( m_EndVoiceId ) {
            const std::uint16_t distance = Distance( *m_ExpectedVoiceId, *m_EndVoiceId );
            if ( distance > 0 && distance < 0x8000U ) {
                return true;
            }
        }
        for ( const auto& [voiceId, frame] : m_Pending ) {
            (void)frame;
            const std::uint16_t distance = Distance( *m_ExpectedVoiceId, voiceId );
            if ( distance > 0 && distance < 0x8000U ) {
                return true;
            }
        }
        return false;
    }

    void VoiceJitterBuffer::Advance() {
        *m_ExpectedVoiceId = static_cast<std::uint16_t>( *m_ExpectedVoiceId + 1U );
    }

} // namespace ts::audio
