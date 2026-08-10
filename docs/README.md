# ts-cli documentation

This directory contains user-, runtime- and protocol-facing documentation for `ts-cli`. It focuses on behavior and architecture rather than documenting individual C++ classes or functions.

## Client

- [Audio and voice](client/audio.md) — PipeWire/Opus pipeline, devices, mute behavior, capture filters and RNNoise
- [Runtime model](client/runtime.md) — UI/network/audio ownership, queues, threading and shutdown
- [Configuration and identity](client/configuration.md) — XDG config location, wire profile and persistent TeamSpeak identity

## Protocol

- [Handshake and session bootstrap](protocol/handshake.md) — connection bootstrap from Init1 through `initserver`
- [Connection lifecycle](protocol/connection_lifecycle.md) — lifecycle, keepalive and disconnect behavior
- [Packet format](protocol/packet_format.md) — low-level packet headers, types and flags
- [Reliability and sequencing](protocol/reliability.md) — ACKs, packet ids, generations, retransmission and ordering
- [Command protocol](protocol/commands.md) — TeamSpeak command rows, escaping, fragmentation and compression
- [Session cryptography](protocol/session_crypto.md) — session encryption and key/nonce derivation
- [Server state](protocol/state.md) — channel/client snapshots and live notifications
- [Text chat](protocol/text_chat.md) — text-message targets and notifications
- [Voice](protocol/voice.md) — voice/whisper wire format, sequencing, crypto and client integration boundary

Some older protocol chapters are still outlines. Status notes at the top of those files indicate where documentation is incomplete.

## Development

Development conventions live in the repository-level [CONTRIBUTING.md](../CONTRIBUTING.md).

## Suggested reading order

For protocol work, start with:

1. [Handshake and session bootstrap](protocol/handshake.md)
2. [Packet format](protocol/packet_format.md)
3. [Reliability and sequencing](protocol/reliability.md)
4. [Command protocol](protocol/commands.md)
5. [Session cryptography](protocol/session_crypto.md)
6. [Server state](protocol/state.md)
7. [Voice](protocol/voice.md)

For client/audio work, start with [Runtime model](client/runtime.md) and [Audio and voice](client/audio.md).
