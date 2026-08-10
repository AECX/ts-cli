# Protocol documentation

The protocol documentation describes TeamSpeak client-protocol behavior as used by `ts-cli`. It focuses on observable wire behavior and protocol concepts rather than C++ implementation details.

## Chapters

1. [Handshake and session bootstrap](handshake.md)
2. [Connection lifecycle](connection_lifecycle.md)
3. [Packet format](packet_format.md)
4. [Reliability and sequencing](reliability.md)
5. [Command protocol](commands.md)
6. [Session cryptography](session_crypto.md)
7. [Server state](state.md)
8. [Text chat](text_chat.md)
9. [Voice](voice.md)

The voice chapter now covers the implemented voice/whisper transport and its boundary with the client audio subsystem. Several older chapters remain outlines and are marked accordingly in their files.

See the [documentation index](../README.md) for client-side documentation, including the [audio pipeline](../client/audio.md) and [runtime model](../client/runtime.md).
