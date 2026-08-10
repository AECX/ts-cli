# Packet format

> Status: outline

This chapter will document the low-level TeamSpeak UDP packet format.

## Planned sections

1. Client-to-server header
2. Server-to-client header
3. Packet IDs and client IDs
4. Packet types
5. Flags: fragmented, new protocol, compressed and unencrypted
6. Init1 packets versus normal packets
7. Packet size limits
8. MAC placement and authenticated metadata
9. Direction-specific differences
10. Examples and annotated packet layouts

Related chapters: [Handshake](handshake.md), [Session cryptography](session_crypto.md), [Reliability and sequencing](reliability.md).
