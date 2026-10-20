#include <algorithm>
#include <audio/audio_engine.hpp>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace ts::audio {

    AudioEngine::AudioEngine( NetworkWakeCallback networkWake ):
        m_NetworkWake( std::move( networkWake ) ), m_CaptureFilter( CreateCaptureFilter( "none" ) ) {
        try {
            m_Backend = CreateAudioBackend();
            m_Backend->Start(
                [this]( std::span<const float> samples ) {
                    OnCapturedSamples( samples );
                },
                [this]( std::span<float> output ) {
                    RenderPlayback( output );
                } );

            std::scoped_lock lock( m_StateMutex );
            m_Available = true;
        } catch ( const std::exception& exception ) {
            std::scoped_lock lock( m_StateMutex );
            m_Error = exception.what();
            m_Backend.reset();
        }

        m_Worker = std::jthread( [this]( std::stop_token stopToken ) {
            Worker( stopToken );
        } );
    }

    AudioEngine::~AudioEngine() {
        if ( m_Worker.joinable() ) {
            m_Worker.request_stop();
            WakeWorker();
            m_Worker.join();
        }

        if ( m_Backend ) {
            m_Backend->Stop();
        }
    }

    std::vector<AudioDevice> AudioEngine::Devices() const {
        RequireAvailable();
        return m_Backend->Devices();
    }

    void AudioEngine::SetInputDevice( std::string_view selector ) {
        RequireAvailable();
        m_Backend->SetInputDevice( selector );

        std::scoped_lock lock( m_StateMutex );
        m_InputSelector = std::string( selector );
    }

    void AudioEngine::SetOutputDevice( std::string_view selector ) {
        RequireAvailable();
        m_Backend->SetOutputDevice( selector );

        std::scoped_lock lock( m_StateMutex );
        m_OutputSelector = std::string( selector );
    }

    void AudioEngine::SetCaptureFilter( std::string_view name ) {
        std::unique_ptr<CaptureFilter> filter = CreateCaptureFilter( name );

        std::scoped_lock lock( m_FilterMutex );
        m_CaptureFilter = std::move( filter );
        m_CaptureFilterName = std::string( name );
    }

    std::vector<std::string> AudioEngine::CaptureFilters() const {
        return AvailableCaptureFilters();
    }

    void AudioEngine::SetActivationThresholdDb( float thresholdDb ) {
        if ( !std::isfinite( thresholdDb ) || thresholdDb < -100.0F || thresholdDb > 0.0F ) {
            throw std::runtime_error( "Audio activation threshold must be between -100 and 0 dBFS" );
        }

        m_ActivationThresholdDb.store( thresholdDb, std::memory_order_release );
        WakeWorker();
    }

    float AudioEngine::ActivationThresholdDb() const {
        return m_ActivationThresholdDb.load( std::memory_order_acquire );
    }

    AudioSettings AudioEngine::Settings() const {
        AudioSettings settings;
        settings.activationThresholdDb = ActivationThresholdDb();

        std::scoped_lock lock( m_StateMutex );
        settings.input = m_InputSelector;
        settings.output = m_OutputSelector;
        {
            std::scoped_lock filterLock( m_FilterMutex );
            settings.captureFilter = m_CaptureFilterName;
        }

        return settings;
    }

    void AudioEngine::SetTransmitEnabled( bool enabled ) {
        if ( enabled ) {
            RequireAvailable();
        }

        std::uint64_t state = m_TransmitState.load( std::memory_order_acquire );

        while ( true ) {
            const bool previous = ( state & 1U ) != 0;

            if ( previous == enabled ) {
                return;
            }

            const std::uint64_t revision = state >> 1U;
            const std::uint64_t next = ( ( revision + 1U ) << 1U ) | ( enabled ? 1U : 0U );

            if ( m_TransmitState.compare_exchange_weak( state, next, std::memory_order_acq_rel, std::memory_order_acquire ) ) {
                break;
            }
        }

        WakeWorker();
        NotifyNetwork();
    }

    bool AudioEngine::TransmitEnabled() const {
        return LoadTransmitState().enabled;
    }

    std::optional<TransmitStateChange> AudioEngine::TakeTransmitStateChange() {
        const TransmitStateChange state = LoadTransmitState();

        if ( state.revision == m_NetworkTransmitRevision ) {
            return std::nullopt;
        }

        m_NetworkTransmitRevision = state.revision;
        return state;
    }

    void AudioEngine::SetPlaybackMuted( bool muted ) {
        if ( m_PlaybackMuted.exchange( muted, std::memory_order_acq_rel ) != muted ) {
            NotifyNetwork();
        }
    }

    bool AudioEngine::PlaybackMuted() const {
        return m_PlaybackMuted.load( std::memory_order_acquire );
    }

    void AudioEngine::SetCaptureMonitor( CaptureMonitor monitor ) {
        if ( monitor != CaptureMonitor::Off ) {
            RequireAvailable();
        }

        m_CaptureMonitor.store( monitor, std::memory_order_release );
        WakeWorker();
    }

    CaptureMonitor AudioEngine::CaptureMonitorMode() const {
        return m_CaptureMonitor.load( std::memory_order_acquire );
    }

    InputLevel AudioEngine::CurrentInputLevel() const {
        return InputLevel { .levelDb = m_InputLevelDb.load( std::memory_order_relaxed ),
                            .voiceActive = m_InputVoiceActive.load( std::memory_order_relaxed ) };
    }

    AudioStatus AudioEngine::Status() const {
        AudioStatus status;
        status.transmitEnabled = TransmitEnabled();
        status.playbackMuted = PlaybackMuted();
        status.captureDrops = m_CaptureDrops.load( std::memory_order_relaxed );
        status.encodedDrops = m_EncodedDrops.load( std::memory_order_relaxed );
        status.receiveDrops = m_ReceiveDrops.load( std::memory_order_relaxed );
        status.playbackUnderruns = m_PlaybackUnderruns.load( std::memory_order_relaxed );

        std::scoped_lock lock( m_StateMutex );
        status.available = m_Available;
        status.input = m_InputSelector;
        status.output = m_OutputSelector;
        {
            std::scoped_lock filterLock( m_FilterMutex );
            status.captureFilter = m_CaptureFilterName;
        }
        status.activationThresholdDb = ActivationThresholdDb();
        status.error = m_Error;
        return status;
    }

    bool AudioEngine::TryPopOutgoing( EncodedFrame& frame ) {
        return m_OutgoingQueue.TryPop( frame );
    }

    bool AudioEngine::SubmitIncoming( const IncomingEncodedFrame& frame ) {
        if ( frame.codec != 4 && frame.codec != 5 ) {
            return false;
        }

        if ( !m_IncomingQueue.TryPush( frame ) ) {
            m_ReceiveDrops.fetch_add( 1, std::memory_order_relaxed );
            return false;
        }

        WakeWorker();
        return true;
    }

    void AudioEngine::SetTalkerSettings( std::uint16_t clientId, const TalkerSettings& settings ) {
        if ( clientId == 0 ) {
            throw std::runtime_error( "Talker client ID cannot be zero" );
        }
        if ( !std::isfinite( settings.volumeDb ) || settings.volumeDb < -60.0F || settings.volumeDb > 12.0F ) {
            throw std::runtime_error( "Talker volume must be between -60 and +12 dB" );
        }

        const float gain = std::pow( 10.0F, settings.volumeDb / 20.0F );
        std::scoped_lock lock( m_TalkerSettingsMutex );
        m_TalkerMixSettings.insert_or_assign( clientId, TalkerMixSettings { .gain = gain, .muted = settings.muted } );
    }

    void AudioEngine::RemoveTalker( std::uint16_t clientId ) {
        {
            std::scoped_lock lock( m_TalkerSettingsMutex );
            m_TalkerMixSettings.erase( clientId );
        }

        if ( !m_TalkerResetQueue.TryPush( clientId ) ) {
            m_ReceiveDrops.fetch_add( 1, std::memory_order_relaxed );
        }
        WakeWorker();
    }

    std::set<std::uint16_t> AudioEngine::MutedTalkers() const {
        std::set<std::uint16_t> muted;
        std::scoped_lock lock( m_TalkerSettingsMutex );

        for ( const auto& [clientId, settings] : m_TalkerMixSettings ) {
            if ( settings.muted ) {
                muted.insert( clientId );
            }
        }

        return muted;
    }

    void AudioEngine::OnCapturedSamples( std::span<const float> samples ) {
        if ( !TransmitEnabled() && CaptureMonitorMode() == CaptureMonitor::Off ) {
            m_CaptureAssemblyOffset = 0;
            return;
        }

        std::size_t offset = 0;

        while ( offset < samples.size() ) {
            const std::size_t remaining = SamplesPerFrame - m_CaptureAssemblyOffset;
            const std::size_t count = std::min( remaining, samples.size() - offset );

            std::copy_n( samples.data() + offset, count, m_CaptureAssembly.samples.data() + m_CaptureAssemblyOffset );

            offset += count;
            m_CaptureAssemblyOffset += count;

            if ( m_CaptureAssemblyOffset != SamplesPerFrame ) {
                continue;
            }

            if ( !m_CaptureQueue.TryPush( m_CaptureAssembly ) ) {
                m_CaptureDrops.fetch_add( 1, std::memory_order_relaxed );
            } else {
                WakeWorker();
            }

            m_CaptureAssemblyOffset = 0;
        }
    }

    void AudioEngine::RenderPlayback( std::span<float> output ) {
        std::size_t written = 0;

        while ( written < output.size() ) {
            if ( m_PlaybackOffset >= SamplesPerFrame ) {
                if ( !m_PlaybackQueue.TryPop( m_PlaybackCurrent ) ) {
                    std::fill( output.begin() + static_cast<std::ptrdiff_t>( written ), output.end(), 0.0F );
                    m_PlaybackUnderruns.fetch_add( 1, std::memory_order_relaxed );
                    return;
                }

                m_PlaybackOffset = 0;
            }

            const std::size_t available = SamplesPerFrame - m_PlaybackOffset;
            const std::size_t count = std::min( available, output.size() - written );

            std::copy_n( m_PlaybackCurrent.samples.data() + m_PlaybackOffset, count, output.data() + written );

            written += count;
            m_PlaybackOffset += count;
        }
    }

    void AudioEngine::Worker( std::stop_token stopToken ) {
        using namespace std::chrono_literals;

        std::unique_ptr<Encoder> encoder;

        try {
            encoder = CreateEncoder();
        } catch ( const std::exception& exception ) {
            std::scoped_lock lock( m_StateMutex );
            m_Error = exception.what();
            m_Available = false;
        }

        TransmitStateChange workerState;
        CaptureMonitor workerMonitor = CaptureMonitor::Off;
        CaptureGate captureGate;
        std::size_t talkStartRemaining = 0;
        auto nextMix = std::chrono::steady_clock::now() + 20ms;
        std::uint64_t observedSignal = m_WorkSignal.load( std::memory_order_acquire );

        try {
            while ( !stopToken.stop_requested() ) {
                if ( encoder ) {
                    ProcessCapture( *encoder, workerState, workerMonitor, captureGate, talkStartRemaining );
                }

                ProcessIncoming();

                const auto now = std::chrono::steady_clock::now();

                std::size_t catchUpFrames = 0;
                while ( now >= nextMix && catchUpFrames < 3 ) {
                    MixPlayback();
                    nextMix += 20ms;
                    ++catchUpFrames;
                }

                if ( now >= nextMix + 100ms ) {
                    nextMix = now + 20ms;
                }

                const std::uint64_t currentSignal = m_WorkSignal.load( std::memory_order_acquire );

                if ( currentSignal != observedSignal ) {
                    observedSignal = currentSignal;
                    continue;
                }

                std::unique_lock lock( m_WorkMutex );
                m_WorkCondition.wait_until( lock, nextMix, [this, stopToken, observedSignal] {
                    return stopToken.stop_requested() || m_WorkSignal.load( std::memory_order_acquire ) != observedSignal;
                } );
                observedSignal = m_WorkSignal.load( std::memory_order_acquire );
            }
        } catch ( const std::exception& exception ) {
            SetTransmitEnabled( false );

            std::scoped_lock lock( m_StateMutex );
            m_Available = false;
            m_Error = exception.what();
        }
    }

    void AudioEngine::ProcessCapture( Encoder& encoder,
                                      TransmitStateChange& workerState,
                                      CaptureMonitor& workerMonitor,
                                      CaptureGate& gate,
                                      std::size_t& talkStartRemaining ) {
        const TransmitStateChange desiredState = LoadTransmitState();
        const CaptureMonitor desiredMonitor = CaptureMonitorMode();

        if ( desiredState.revision != workerState.revision || desiredMonitor != workerMonitor ) {
            const bool wasTalking = gate.Active();
            const bool wasTransmitting = workerState.enabled;
            const bool wasLoopback = workerMonitor == CaptureMonitor::Loopback;

            workerState = desiredState;
            workerMonitor = desiredMonitor;
            encoder.Reset();
            gate.Reset();
            m_CaptureQueue.Clear();
            talkStartRemaining = 0;
            m_InputLevelDb.store( -100.0F, std::memory_order_relaxed );
            m_InputVoiceActive.store( false, std::memory_order_relaxed );

            if ( wasTalking && wasTransmitting ) {
                QueueTalkEnd( workerState.revision );
            }

            if ( wasLoopback ) {
                m_Talkers.erase( LoopbackTalkerId );
            }
        }

        if ( !workerState.enabled && workerMonitor == CaptureMonitor::Off ) {
            return;
        }

        const bool loopback = workerMonitor == CaptureMonitor::Loopback;
        PcmFrame pcm;

        while ( m_CaptureQueue.TryPop( pcm ) ) {
            {
                std::scoped_lock lock( m_FilterMutex );
                m_CaptureFilter->Process( pcm );
            }

            const bool wasTalking = gate.Active();
            const bool shouldTransmit = gate.Process( pcm, ActivationThresholdDb() );
            m_InputLevelDb.store( gate.LevelDb(), std::memory_order_relaxed );
            m_InputVoiceActive.store( shouldTransmit, std::memory_order_relaxed );

            if ( !shouldTransmit ) {
                if ( wasTalking ) {
                    if ( workerState.enabled ) {
                        QueueTalkEnd( workerState.revision );
                    }

                    if ( loopback ) {
                        EncodedFrame end;
                        end.talkEnd = true;
                        PlayLoopback( end );
                    }

                    encoder.Reset();
                    talkStartRemaining = 0;
                }
                continue;
            }

            if ( !wasTalking ) {
                encoder.Reset();
                talkStartRemaining = TalkStartPackets;
            }

            EncodedFrame encoded;
            encoded.size = encoder.Encode( pcm, encoded.data );
            encoded.transmitRevision = workerState.revision;
            encoded.talkStart = talkStartRemaining != 0;

            if ( talkStartRemaining != 0 ) {
                --talkStartRemaining;
            }

            if ( loopback ) {
                PlayLoopback( encoded );
            }

            if ( !workerState.enabled ) {
                continue;
            }

            if ( !m_OutgoingQueue.TryPush( encoded ) ) {
                /*
                 * Audio is latency-sensitive. If the network side is behind,
                 * discard this old frame instead of building an ever-growing
                 * speech backlog.
                 */
                m_EncodedDrops.fetch_add( 1, std::memory_order_relaxed );
            } else {
                NotifyNetwork();
            }
        }
    }

    void AudioEngine::PlayLoopback( const EncodedFrame& frame ) {
        /*
         * Feed the encoded frame through the same jitter buffer/decoder path
         * as a remote talker, so the test sounds like what others hear.
         */
        IncomingEncodedFrame incoming;
        incoming.clientId = LoopbackTalkerId;
        incoming.voiceId = m_LoopbackVoiceId++;
        incoming.codec = 4;
        incoming.size = frame.size;
        incoming.talkStart = frame.talkStart;
        incoming.talkEnd = frame.talkEnd;
        std::copy_n( frame.data.begin(), frame.size, incoming.data.begin() );

        TalkerState& talker = m_Talkers[LoopbackTalkerId];

        if ( !talker.decoder ) {
            talker.decoder = CreateDecoder();
        }

        talker.jitter.Push( incoming );
    }

    void AudioEngine::QueueTalkEnd( std::uint64_t transmitRevision ) {
        EncodedFrame end;
        end.transmitRevision = transmitRevision;
        end.talkEnd = true;

        if ( !m_OutgoingQueue.TryPush( end ) ) {
            m_EncodedDrops.fetch_add( 1, std::memory_order_relaxed );
        } else {
            NotifyNetwork();
        }
    }

    TransmitStateChange AudioEngine::LoadTransmitState() const {
        const std::uint64_t state = m_TransmitState.load( std::memory_order_acquire );
        return TransmitStateChange { .enabled = ( state & 1U ) != 0, .revision = state >> 1U };
    }

    void AudioEngine::Notify( NotificationType type ) {
        m_NotificationRequested.store( static_cast<std::uint8_t>( type ), std::memory_order_release );
        WakeWorker();
    }

    void AudioEngine::ProcessIncoming() {
        std::uint16_t resetClientId = 0;
        while ( m_TalkerResetQueue.TryPop( resetClientId ) ) {
            m_Talkers.erase( resetClientId );
        }

        IncomingEncodedFrame frame;

        while ( m_IncomingQueue.TryPop( frame ) ) {
            TalkerState& talker = m_Talkers[frame.clientId];
            if ( !talker.decoder ) {
                talker.decoder = CreateDecoder();
            }
            talker.jitter.Push( frame );
        }
    }

    void AudioEngine::MixPlayback() {
        PcmFrame mixed;
        std::size_t contributors = 0;

        std::map<std::uint16_t, TalkerMixSettings> mixSettings;
        {
            std::scoped_lock lock( m_TalkerSettingsMutex );
            mixSettings = m_TalkerMixSettings;
        }

        const bool playbackMuted = PlaybackMuted();

        for ( auto& [clientId, talker] : m_Talkers ) {
            if ( !talker.decoder ) {
                continue;
            }

            while ( talker.decodedSamples.size() < SamplesPerFrame ) {
                const JitterPullResult item = talker.jitter.Pull();
                if ( item.kind == JitterPullKind::Wait ) {
                    break;
                }
                if ( item.kind == JitterPullKind::End ) {
                    talker.decoder->Reset();
                    talker.lastPacketSamples = SamplesPerFrame;

                    if ( !talker.decodedSamples.empty() ) {
                        talker.decodedSamples.resize( SamplesPerFrame, 0.0F );
                    }
                    break;
                }

                if ( item.streamStart ) {
                    talker.decoder->Reset();
                    talker.decodedSamples.clear();
                    talker.lastPacketSamples = SamplesPerFrame;
                }

                DecodedAudio decoded;
                DecodeMode decodeMode = DecodeMode::Normal;
                std::size_t recoverySamples = 0;
                std::span<const std::byte> encoded( item.frame.data.data(), item.frame.size );

                if ( item.kind == JitterPullKind::PacketLoss ) {
                    recoverySamples = talker.lastPacketSamples;
                    if ( item.hasRecoveryFrame ) {
                        decodeMode = DecodeMode::ForwardErrorCorrection;
                    } else {
                        decodeMode = DecodeMode::PacketLoss;
                        encoded = {};
                    }
                }

                std::size_t decodedSamples = 0;

                try {
                    decodedSamples = talker.decoder->Decode( encoded, decoded, decodeMode, recoverySamples );
                } catch ( const std::exception& ) {
                    /*
                     * A malformed or unsupported remote packet must not take
                     * down capture/playback for the entire client. Reset only
                     * this talker and wait for the next talk burst.
                     */
                    talker.decoder->Reset();
                    talker.jitter.Reset();
                    talker.decodedSamples.clear();
                    talker.lastPacketSamples = SamplesPerFrame;
                    m_ReceiveDrops.fetch_add( 1, std::memory_order_relaxed );
                    break;
                }

                if ( decodedSamples == 0 ) {
                    continue;
                }
                if ( decodedSamples > decoded.samples.size() ) {
                    throw std::runtime_error( "Audio decoder returned too many samples" );
                }

                if ( decodeMode == DecodeMode::Normal ) {
                    talker.lastPacketSamples = decodedSamples;
                }

                talker.decodedSamples.insert( talker.decodedSamples.end(),
                                              decoded.samples.begin(),
                                              decoded.samples.begin() + decodedSamples );
            }

            if ( talker.decodedSamples.size() < SamplesPerFrame ) {
                continue;
            }

            PcmFrame decoded;
            for ( std::size_t index = 0; index < SamplesPerFrame; ++index ) {
                decoded.samples[index] = talker.decodedSamples.front();
                talker.decodedSamples.pop_front();
            }

            const auto configured = mixSettings.find( clientId );
            const TalkerMixSettings settings = configured == mixSettings.end() ? TalkerMixSettings {} : configured->second;

            if ( settings.muted || ( playbackMuted && clientId != LoopbackTalkerId ) ) {
                continue;
            }

            for ( std::size_t index = 0; index < SamplesPerFrame; ++index ) {
                mixed.samples[index] += decoded.samples[index] * settings.gain;
            }
            ++contributors;
        }

        const std::uint8_t request = m_NotificationRequested.exchange( NoNotificationRequested, std::memory_order_acq_rel );

        if ( request != NoNotificationRequested ) {
            m_NotificationSynthesizer.Start( static_cast<NotificationType>( request ) );
        }

        const bool notification = m_NotificationSynthesizer.Mix( mixed );

        if ( contributors == 0 && !notification ) {
            return;
        }

        for ( float& sample : mixed.samples ) {
            sample = std::clamp( sample, -1.0F, 1.0F );
        }

        (void)m_PlaybackQueue.TryPush( mixed );
    }

    void AudioEngine::WakeWorker() {
        m_WorkSignal.fetch_add( 1, std::memory_order_release );
        m_WorkCondition.notify_one();
    }

    void AudioEngine::NotifyNetwork() const {
        if ( m_NetworkWake ) {
            m_NetworkWake();
        }
    }

    void AudioEngine::RequireAvailable() const {
        std::scoped_lock lock( m_StateMutex );

        if ( m_Available && m_Backend ) {
            return;
        }

        if ( m_Error.empty() ) {
            throw std::runtime_error( "Audio backend is unavailable" );
        }

        throw std::runtime_error( "Audio backend is unavailable: " + m_Error );
    }

} // namespace ts::audio
