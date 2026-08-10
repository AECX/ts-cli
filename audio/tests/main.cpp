#include <audio/audio_engine.hpp>
#include <audio/audio_types.hpp>
#include <audio/capture_filter.hpp>
#include <audio/capture_gate.hpp>
#include <audio/spsc_ring.hpp>
#include <audio/voice_jitter_buffer.hpp>
#include <cstdlib>
#include <iostream>

int main() {
    ts::audio::SpscRing<int, 4> ring;
    int value = 0;

    if ( !ring.TryPush( 1 ) || !ring.TryPush( 2 ) || !ring.TryPop( value ) || value != 1 ) {
        std::cerr << "SPSC ring test failed\n";
        return EXIT_FAILURE;
    }

    const auto filters = ts::audio::AvailableCaptureFilters();
    if ( filters.empty() || filters.front() != "none" ) {
        std::cerr << "Audio capture-filter availability test failed\n";
        return EXIT_FAILURE;
    }

    auto passthrough = ts::audio::CreateCaptureFilter( "none" );
    ts::audio::PcmFrame filterFrame;
    filterFrame.samples[0] = 0.5F;
    passthrough->Process( filterFrame );
    if ( filterFrame.samples[0] != 0.5F ) {
        std::cerr << "Passthrough capture-filter test failed\n";
        return EXIT_FAILURE;
    }

    ts::audio::CaptureGate gate;
    ts::audio::PcmFrame quietFrame;
    quietFrame.samples.fill( 0.001F );
    if ( gate.Process( quietFrame, -45.0F ) ) {
        std::cerr << "Capture activation gate opened for a quiet frame\n";
        return EXIT_FAILURE;
    }

    ts::audio::PcmFrame speechFrame;
    speechFrame.samples.fill( 0.05F );
    if ( !gate.Process( speechFrame, -45.0F ) || !gate.Active() ) {
        std::cerr << "Capture activation gate did not open for speech\n";
        return EXIT_FAILURE;
    }

    for ( std::size_t index = 0; index < 15; ++index ) {
        if ( !gate.Process( quietFrame, -45.0F ) ) {
            std::cerr << "Capture activation hangover ended too early\n";
            return EXIT_FAILURE;
        }
    }

    if ( gate.Process( quietFrame, -45.0F ) || gate.Active() ) {
        std::cerr << "Capture activation hangover did not close\n";
        return EXIT_FAILURE;
    }

    {
        ts::audio::VoiceJitterBuffer jitter;
        ts::audio::IncomingEncodedFrame frame10 { .clientId = 1, .voiceId = 10, .codec = 4, .talkStart = true };
        ts::audio::IncomingEncodedFrame frame11 { .clientId = 1, .voiceId = 11, .codec = 4 };
        ts::audio::IncomingEncodedFrame frame12 { .clientId = 1, .voiceId = 12, .codec = 4 };
        ts::audio::IncomingEncodedFrame end13 { .clientId = 1, .voiceId = 13, .codec = 4, .talkEnd = true };

        jitter.Push( frame10 );
        jitter.Push( frame12 );
        jitter.Push( frame11 );
        jitter.Push( end13 );

        const auto first = jitter.Pull();
        const auto second = jitter.Pull();
        const auto third = jitter.Pull();
        const auto end = jitter.Pull();

        if ( first.kind != ts::audio::JitterPullKind::Frame || first.frame.voiceId != 10 || !first.streamStart ||
             second.kind != ts::audio::JitterPullKind::Frame || second.frame.voiceId != 11 ||
             third.kind != ts::audio::JitterPullKind::Frame || third.frame.voiceId != 12 ||
             end.kind != ts::audio::JitterPullKind::End ) {
            std::cerr << "Voice jitter reorder test failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        ts::audio::VoiceJitterBuffer jitter;
        ts::audio::IncomingEncodedFrame frame30 { .clientId = 1, .voiceId = 30, .codec = 5, .talkStart = true };
        ts::audio::IncomingEncodedFrame frame31 { .clientId = 1, .voiceId = 31, .codec = 5 };
        ts::audio::IncomingEncodedFrame frame32 { .clientId = 1, .voiceId = 32, .codec = 5 };

        jitter.Push( frame30 );
        jitter.Push( frame31 );
        jitter.Push( frame32 );

        if ( jitter.Pull().frame.codec != 5 || jitter.Pull().frame.codec != 5 || jitter.Pull().frame.codec != 5 ||
             jitter.Pull().kind != ts::audio::JitterPullKind::Wait ) {
            std::cerr << "Opus Music jitter/backend routing test failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        ts::audio::VoiceJitterBuffer jitter;
        ts::audio::IncomingEncodedFrame frame20 { .clientId = 1, .voiceId = 20, .codec = 4, .talkStart = true };
        ts::audio::IncomingEncodedFrame frame22 { .clientId = 1, .voiceId = 22, .codec = 4 };
        ts::audio::IncomingEncodedFrame end23 { .clientId = 1, .voiceId = 23, .codec = 4, .talkEnd = true };

        jitter.Push( frame20 );
        jitter.Push( frame22 );
        jitter.Push( end23 );

        const auto first = jitter.Pull();
        const auto loss = jitter.Pull();
        const auto recovered = jitter.Pull();
        const auto end = jitter.Pull();

        if ( first.kind != ts::audio::JitterPullKind::Frame || first.frame.voiceId != 20 ||
             loss.kind != ts::audio::JitterPullKind::PacketLoss || !loss.hasRecoveryFrame || loss.frame.voiceId != 22 ||
             recovered.kind != ts::audio::JitterPullKind::Frame || recovered.frame.voiceId != 22 ||
             end.kind != ts::audio::JitterPullKind::End ) {
            std::cerr << "Voice jitter loss/FEC test failed\n";
            return EXIT_FAILURE;
        }
    }

    {
        ts::audio::VoiceJitterBuffer jitter;
        ts::audio::IncomingEncodedFrame beforeWrap { .clientId = 1, .voiceId = 65534, .codec = 4, .talkStart = true };
        ts::audio::IncomingEncodedFrame last { .clientId = 1, .voiceId = 65535, .codec = 4 };
        ts::audio::IncomingEncodedFrame wrapped { .clientId = 1, .voiceId = 0, .codec = 4 };
        ts::audio::IncomingEncodedFrame end { .clientId = 1, .voiceId = 1, .codec = 4, .talkEnd = true };

        jitter.Push( beforeWrap );
        jitter.Push( last );
        jitter.Push( wrapped );
        jitter.Push( end );

        if ( jitter.Pull().frame.voiceId != 65534 || jitter.Pull().frame.voiceId != 65535 || jitter.Pull().frame.voiceId != 0 ||
             jitter.Pull().kind != ts::audio::JitterPullKind::End ) {
            std::cerr << "Voice jitter wrap test failed\n";
            return EXIT_FAILURE;
        }
    }

    if ( ts::audio::MaxDecodedSamples != 5760 ) {
        std::cerr << "Maximum Opus packet duration test failed\n";
        return EXIT_FAILURE;
    }

    ts::audio::AudioEngine engine;
    const ts::audio::AudioStatus status = engine.Status();

    /*
     * status.available is intentionally not asserted here: it reflects
     * whether the compiled-in AudioBackend could actually start (a real
     * device session, or nothing at all for the stub backend under
     * TS_AUDIO_STUB_DEPS=ON), which legitimately varies by build
     * configuration and environment. The fields below must be deterministic
     * regardless of backend.
     */
    if ( status.transmitEnabled || status.captureFilter != "none" || status.activationThresholdDb != -45.0F ) {
        std::cerr << "Audio engine initial-state test failed\n";
        return EXIT_FAILURE;
    }

    ts::audio::IncomingEncodedFrame opusVoice { .clientId = 10, .voiceId = 1, .codec = 4, .talkEnd = true };
    ts::audio::IncomingEncodedFrame opusMusic { .clientId = 11, .voiceId = 1, .codec = 5, .talkEnd = true };
    ts::audio::IncomingEncodedFrame unsupportedCodec { .clientId = 12, .voiceId = 1, .codec = 3, .talkEnd = true };
    if ( !engine.SubmitIncoming( opusVoice ) || !engine.SubmitIncoming( opusMusic ) ||
         engine.SubmitIncoming( unsupportedCodec ) ) {
        std::cerr << "Opus Voice/Music backend acceptance test failed\n";
        return EXIT_FAILURE;
    }

    const auto initialState = engine.TakeTransmitStateChange();

    if ( !initialState || initialState->enabled || initialState->revision == 0 ) {
        std::cerr << "Audio initial transmit transition test failed\n";
        return EXIT_FAILURE;
    }

    if ( engine.TakeTransmitStateChange() ) {
        std::cerr << "Audio transmit transition repeated without a change\n";
        return EXIT_FAILURE;
    }

    /*
     * Enabling transmission requires a working backend (RequireAvailable()
     * inside SetTransmitEnabled(true)), which the stub backend under
     * TS_AUDIO_STUB_DEPS=ON intentionally never provides. Exercise this
     * transition only when a real backend actually started.
     */
    if ( status.available ) {
        engine.SetTransmitEnabled( true );
        const auto enabledState = engine.TakeTransmitStateChange();

        if ( !engine.TransmitEnabled() || !enabledState || !enabledState->enabled ||
             enabledState->revision <= initialState->revision ) {
            std::cerr << "Audio transmit enable transition test failed\n";
            return EXIT_FAILURE;
        }

        engine.SetTransmitEnabled( false );
        const auto disabledState = engine.TakeTransmitStateChange();

        if ( engine.TransmitEnabled() || !disabledState || disabledState->enabled ||
             disabledState->revision <= enabledState->revision ) {
            std::cerr << "Audio transmit disable transition test failed\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "audio tests passed\n";
    return EXIT_SUCCESS;
}
