#ifndef TS_PROTOCOL_SESSION_AUDIO_STATE_HPP
#define TS_PROTOCOL_SESSION_AUDIO_STATE_HPP

namespace ts::protocol {

    /*
     * Locally observable microphone/speaker state, published to the server so
     * other clients can render the usual muted/no-hardware indicators.
     *
     * This is presentation state only: it does not gate voice transmission,
     * which the caller controls by simply not calling SendVoice.
     */
    struct AudioState {
        bool inputHardware = true;
        bool outputHardware = true;

        bool inputMuted = false;
    };

} // namespace ts::protocol

#endif // TS_PROTOCOL_SESSION_AUDIO_STATE_HPP
