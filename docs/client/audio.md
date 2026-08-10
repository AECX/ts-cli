# Audio and voice

`ts-cli` has a native audio path built around Opus, with a platform-specific capture/render backend: PipeWire on Linux, WASAPI on Windows. Microphone capture and playback live in the `audio/` subproject; TeamSpeak packet construction, encryption and voice sequencing remain in `protocol/`.

Everything above the backend — the audio engine, Opus codec, jitter buffer, capture filters, mixing, and per-user gain/mute — is identical on both platforms; only device I/O differs. See [Platform backends](../../CONTRIBUTING.md#platform-backends) for how `audio::AudioBackend` is structured to make that split possible.

## Runtime behavior

The audio engine is created after the TeamSpeak connection has been established. Once its backend/codec are ready, the network runtime publishes the resulting hardware and mute state to the server.

Microphone transmission starts disabled. Use:

```text
/unmute
```

to begin transmitting and:

```text
/mute
```

to stop. These are aliases for:

```text
/audio transmit on
/audio transmit off
```

Muting is not implemented as a cosmetic local flag. The audio engine advances a transmit-state revision whenever transmission changes. Encoded frames carry the revision that produced them, allowing the network thread to reject queued frames from an older transmit state. This prevents microphone audio that was already queued before `/mute` from leaking afterward.

When a talk burst ends, the audio worker emits an empty encoded frame as the TeamSpeak talk-end marker. A new talk burst begins with a short run of packets carrying the talk-start flag.

## Capture path

Outgoing voice follows this path:

```text
PipeWire capture
    -> 48 kHz mono float PCM
    -> capture filter
    -> Opus encoder
    -> audio/network SPSC queue
    -> TeamSpeak Voice packet
    -> session encryption/UDP
```

The current frame size is 20 ms at 48 kHz mono, or 960 PCM samples per frame.

Capture filtering runs on the audio worker rather than the backend's own capture callback. Keeping expensive DSP work off the backend callback reduces the risk of blocking real-time capture.

On Windows, the backend is WASAPI in shared mode instead of PipeWire: `audio/src/win32/wasapi_backend.cpp` requests the exact 48 kHz mono float format directly from `IAudioClient` (relying on WASAPI's shared-mode format conversion rather than a custom resampler) and runs capture and render on their own dedicated threads, each driven by its own WASAPI-signaled event — separate from the audio worker thread described below. Device enumeration and selection go through a third control thread that owns the `IMMDeviceEnumerator`. From the capture filter stage onward, the path is identical to Linux.

## Playback path

Incoming voice follows the inverse ownership boundary:

```text
TeamSpeak Voice/VoiceWhisper packet
    -> protocol decode
    -> audio/network SPSC queue
    -> per-client voice-id jitter buffer
    -> per-client Opus decoder
    -> per-client local mute/gain
    -> PCM mixing
    -> playback queue
    -> backend output (PipeWire on Linux, WASAPI on Windows)
```

The audio engine tracks jitter, decoder state and queued decoded PCM independently for each talking client. Packets are reordered by TeamSpeak's inner 16-bit voice id before decode. A small bounded prebuffer absorbs ordinary network reordering; confirmed gaps use Opus in-band FEC from the following packet when available, then packet-loss concealment when FEC is unavailable. Voice-id wraparound is handled explicitly.

Incoming Opus packet duration is not assumed to be 20 ms. TeamSpeak channel latency settings can produce larger Opus packets, so the decoder accepts up to Opus's 120 ms maximum and splits the decoded PCM into the client's fixed 20 ms playback quanta. This is important for Opus Voice channels as well as Opus Music channels. A malformed packet resets only that talker's receive state instead of marking the entire audio backend unavailable.

## Devices

List discovered input and output devices with:

```text
/audio devices
```

The selector `default` follows the system default (on Windows, specifically the *communications* device role, which is what Windows itself expects a VoIP application to use — usually the same endpoint as the console default unless the user has explicitly set a separate one). Devices can also be selected by the id or name shown in the device listing:

```text
/audio input default
/audio input <id>
/audio input <name>

/audio output default
/audio output <id>
/audio output <name>
```

Inspect the active selections and counters with:

```text
/audio status
```

The status line includes backend availability, selected input/output, active capture filter, transmit state and capture/encoded/receive drop counters.

## Per-user playback volume

Remote users can be attenuated, amplified or locally muted without affecting anyone else in the mix:

```text
/user <client>
/user <client> volume -8
/user <client> volume 75%
/user <client> volume reset
/user <client> mute
/user <client> unmute
```

The nickname or `#<client-id>` is only a live selector. Settings are persisted by the remote TeamSpeak unique identity under `users/`, so reconnecting with a different client id or nickname does not lose the preference. Ambiguous nicknames are rejected.

Gain/mute is applied after that talker's Opus decode and before summing talkers together. Muted users are still decoded so their decoder/jitter timeline remains synchronized; they simply do not contribute PCM to the output mix. See [Configuration and identity](configuration.md) for the file layout.

## Capture filters

Capture filters operate only on outgoing microphone PCM before Opus encoding. They do not modify received voice or playback.

List filters compiled into the current binary:

```text
/audio filter
```

Select one with:

```text
/audio filter <name>
```

The `none` filter is always present and passes PCM through unchanged.

### RNNoise

RNNoise support is optional at build time. CMake checks for the `rnnoise` pkg-config module; when found, the `rnnoise` capture filter is compiled into the audio library.

Enable it at runtime with:

```text
/audio filter rnnoise
```

RNNoise operates on fixed 480-sample blocks at 48 kHz. A `ts-cli` capture frame contains 960 samples, so the filter processes each 20 ms microphone frame as two consecutive 480-sample blocks before handing the frame to Opus.

PipeWire PCM is normalized floating-point audio. The RNNoise adapter scales samples to the PCM amplitude range expected by RNNoise, processes the block, then scales and clamps the result back to normalized `[-1, 1]` floats.

If RNNoise was not present when the binary was built, `rnnoise` will not appear in `/audio filter`, and selecting it returns an unavailable-filter error. The rest of the audio subsystem continues to work without it.

## Microphone activation

Unmuting permits transmission; it does not force a continuous voice stream. The capture worker measures the RMS level of each filtered 20 ms microphone frame and only starts a TeamSpeak talk burst when the configured activation threshold is reached.

Use:

```text
/audio threshold -45
```

The value is dBFS in the range `-100` through `0`. Lower values are more sensitive. The default is `-45 dBFS`. A 300 ms hangover keeps brief gaps between words from repeatedly closing and reopening the voice stream. When the hangover expires, ts-cli sends the empty end-of-talk voice packet and remains unmuted but silent until the threshold is crossed again.

The threshold is evaluated after capture filtering. With RNNoise enabled, this prevents much of the removed background noise from influencing microphone activation. Device, filter, and threshold changes are persisted to `config.conf`; mute state is not.

## Threading and ownership

Audio and protocol responsibilities deliberately remain separate:

- Backend callbacks capture/render samples and avoid protocol work — PipeWire's `process` callbacks on Linux, WASAPI's capture/render events (on their own dedicated threads) on Windows.
- The audio worker owns Opus encode/decode, filtering and mixing.
- SPSC queues move fixed-size audio data between the audio worker/backend and network side.
- The network runtime is the only side that calls TeamSpeak connection/session methods.
- TeamSpeak packet sequence numbers, crypto and packet validation stay inside `protocol/`.

This preserves the project's single-network-thread state model while allowing audio DSP to run independently.

## Current limitations

Input/output selection, capture-filter selection and activation threshold are persisted to `config.conf`. Mute state is session-only and starts disabled on every launch.

The receive jitter buffer is deliberately small and bounded rather than adaptive like a mature desktop VoIP client. Device hotplug/recovery and DSP chaining are also intentionally minimal at this stage — on both platforms, a device that changes while the client is running needs `/audio input`/`/audio output` to pick it up rather than switching automatically. WASAPI exclusive mode is not used; the Windows backend is shared-mode only.

Related chapters: [Runtime model](runtime.md), [Voice protocol](../protocol/voice.md).
