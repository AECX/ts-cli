# Voice

> Status: packet transport and client audio integration implemented

The protocol layer handles TeamSpeak `Voice` and `VoiceWhisper` datagrams. Capture/playback devices, Opus codec instances, filtering, mixing and buffering stay outside `protocol/` in the audio/client layers.

## Client voice payloads

A normal client `Voice` payload contains:

1. voice id;
2. codec id;
3. encoded codec payload.

The current client audio path sends Opus frames produced from 48 kHz mono, 20 ms microphone PCM frames. The current channel's codec id is preserved on the TeamSpeak voice packet, so both Opus Voice (`4`) and Opus Music (`5`) channels use the same audio backend/transport path.

An empty encoded payload marks the end of a talk burst. At the beginning of a new talk burst, the client marks an initial run of voice packets with TeamSpeak's talk-start flag (`Compressed` on voice packets).

## Voice whisper payloads

Client `VoiceWhisper` supports both TeamSpeak target forms.

Direct whispers contain channel/client target counts, followed by the target channel ids and client ids, then the encoded voice payload.

Group whispers contain the group type, group target and 64-bit target id followed by the encoded voice payload. Group whispers set the TeamSpeak `NewProtocol` packet flag.

The protocol encoder writes 64-bit target identifiers in network byte order and validates target counts, codec values and datagram size before sending.

## Incoming voice

Server `Voice` and `VoiceWhisper` payloads are decoded into `VoiceFrame`. A received whisper frame is tagged with `VoiceFrame::whisper`, allowing higher layers to distinguish whisper audio without reparsing the packet.

The client runtime forwards the encoded frame, codec id, talker id and inner voice id to `AudioEngine`. The audio worker maintains a bounded jitter buffer and Opus decoder per client. It reorders voice ids, uses the following packet for Opus in-band FEC when a packet is missing, falls back to packet-loss concealment, and then mixes fixed 20 ms PCM playback quanta.

Incoming Opus packets are allowed to decode to as much as 120 ms of 48 kHz PCM. The audio layer no longer assumes that every TeamSpeak Opus packet contains exactly 20 ms; this permits channel codec latency configurations that aggregate more audio into one packet. Decoder failures are isolated to the offending talker so a malformed/unsupported remote stream cannot disable microphone capture or playback globally.

Fragmented voice packets are rejected. Voice is latency-sensitive and is not treated as a splittable/reliable transport class.

## Sequencing

`Voice` and `VoiceWhisper` maintain their own packet-sequence state in `SessionTransport`. They are not inserted into the reliable command retransmission queue.

The voice id inside the encoded voice payload is separate from the outer TeamSpeak packet sequence. The client audio/network path therefore tracks talk/codec framing independently from the packet sequence owned by the protocol transport.

## Encryption and authentication

`SessionTransport` supports encrypted and unencrypted voice packets through the same session-crypto validation used by the rest of the transport.

Unencrypted incoming voice packets are not simply accepted as plaintext: they must still carry the session shared MAC expected by the protocol implementation.

## Mute and hardware state

Microphone mute is reflected both locally and to the TeamSpeak server.

When the audio engine becomes available, the network runtime publishes the current input/output hardware availability. A mute transition updates `client_input_muted` and also changes the audio engine's transmit revision.

Encoded microphone frames are tagged with that revision. The network runtime discards frames belonging to an older state, preventing queued speech from being sent after `/mute`. A matching empty talk-end frame is allowed through to close the active talk burst.

## Layer boundaries

The ownership split is intentional:

- `protocol/` owns TeamSpeak voice packet formatting, packet sequence state, encryption/authentication and receive parsing;
- `audio/` owns PipeWire, Opus, capture filters, decoding/mixing and bounded audio queues;
- `client/` bridges the two on the network thread and exposes runtime controls.

This keeps Linux audio dependencies out of the protocol subproject and keeps TeamSpeak session state out of real-time audio callbacks.

See [Audio and voice](../client/audio.md) for the user/runtime side of the pipeline.
