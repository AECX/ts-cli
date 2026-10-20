# Runtime model

`ts-cli` separates user interaction (the terminal client, or the [GTK client](gtk.md)), TeamSpeak network state and audio processing so protocol objects are not concurrently mutated from unrelated threads.

## Main/UI thread

The main thread owns the interactive terminal loop. It reads input, uses the command parser/handler to turn it into client actions and renders runtime events through the presentation layer.

The GTK client's main thread runs the GTK main loop instead and drains the event queue from a short GLib timer. It reuses the same command handler for slash commands, with the window as its `cli::CommandOutput` in place of the terminal presentation.

The UI does not directly manipulate `protocol::Connection`. Commands that affect TeamSpeak state are pushed to the action queue for the network runtime to execute.

Audio control commands are the exception at the API boundary: the command handler can request changes such as input/output device, capture filter or transmit state through `AudioEngine`. Network-visible mute/hardware state is still published by the network runtime rather than by the UI thread.

## Network runtime

`NetworkRuntime` owns the long-lived network thread and runs `protocol::Connection::Run`.

Its cycle callback performs three jobs:

1. publish current-channel/nickname changes (and, when enabled, server snapshots and talk state) to the UI event queue;
2. drain client actions and apply them to the connection;
3. synchronize audio state and outgoing encoded voice with the TeamSpeak connection.

This keeps packet sequencing, crypto state, server state stores and command processing under one network-side owner.

A front end that renders a live channel/client tree calls `NetworkRuntime::EnableLiveStateEvents()` before starting the runtime. The network thread then copies the channel and client stores into a self-contained `ServerSnapshot` after events and cycles, and pushes a `ServerSnapshotEvent` only when it differs from the last one published. The UI thread renders those copies and never reads protocol state directly. In the same mode it tracks who is transmitting, from incoming voice packets and its own outgoing frames: a client talks from its first packet until its talk-end packet, or until no packet arrived for 500 ms (talk-end packets can be lost). Changes are published as a `TalkStateEvent`. The terminal client does not enable this.

Incoming protocol events are forwarded to the UI except voice events, which retain their encoded payload/voice id and are submitted to the audio engine. Decoding stays in the audio worker so packet reordering, Opus state and playback timing remain per talker.

## Action and event queues

The application uses queues instead of allowing the UI and network threads to call each other's stateful objects directly.

```text
terminal/UI
    | ClientAction
    v
action queue
    |
    v
network runtime -> protocol::Connection
    |
    | RuntimeEvent
    v
event queue
    |
    v
terminal presentation
```

This is particularly important for TeamSpeak packet ids, generations, retransmission state and channel/client stores, all of which assume serialized network-side access.

## Audio worker and backend callbacks

`AudioEngine` owns a worker thread plus the platform backend's capture/playback callbacks (PipeWire on Linux, WASAPI on Windows — see [Audio and voice](audio.md)).

The backend callbacks are intentionally narrow:

- capture callbacks assemble normalized float samples into fixed 20 ms frames and enqueue them;
- playback callbacks copy already-mixed PCM from the playback queue.

The audio worker performs the heavier work:

- capture filtering;
- Opus encoding;
- per-client voice-id reordering/jitter buffering;
- variable-duration Opus decoding, in-band FEC and packet-loss concealment;
- per-client local gain/mute before mixing;
- mixing decoded talkers into playback frames.

SPSC rings provide bounded handoff between these stages. When an outgoing queue is full, stale speech is dropped rather than allowing latency to grow without bound.

## Mute synchronization

Microphone transmission is revisioned. Every transition between muted and unmuted advances a transmit revision, and each encoded outgoing frame records the revision under which it was created.

The network runtime only transmits frames matching the current revision. This solves two transition races:

- PCM/Opus frames queued just before `/mute` cannot leak after the mute request;
- an old talk-end marker cannot terminate a newly started talk burst after `/unmute`.

The network runtime also publishes audio hardware availability, `client_input_muted` and `client_output_muted` (speaker mute) through the TeamSpeak session state.

## Shutdown

A stop request wakes the connection, lets the network thread leave its run loop and closes the action/event queues. The owning thread joins the network runtime before process exit. `AudioEngine` uses a `std::jthread`, so its worker participates in structured stop/join behavior during destruction.

Related chapters: [Audio and voice](audio.md), [Connection lifecycle](../protocol/connection_lifecycle.md), [Reliability and sequencing](../protocol/reliability.md).
