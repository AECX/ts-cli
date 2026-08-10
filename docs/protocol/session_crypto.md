# Session cryptography

> Status: outline

This chapter will document TeamSpeak's authenticated packet encryption after the cryptographic bootstrap.

## Planned sections

1. AES-EAX overview in TeamSpeak
2. Eight-byte packet MAC
3. Authenticated packet metadata
4. Session material produced by the modern handshake
5. Direction/type/packet-ID/generation key and nonce derivation
6. Client encryption
7. Server decryption
8. Packet-ID rollover and crypto generations
9. Encrypted packet types
10. Unencrypted Ping/Pong
11. Voice encryption policy
12. Bootstrap AES-EAX versus session AES-EAX
13. Golden-vector testing strategy

Start with [Handshake and session bootstrap](handshake.md).
