# Reliability and sequencing

> Status: outline

TeamSpeak builds reliable ordered command delivery on top of UDP.

## Planned sections

1. Per-packet-type sequence state
2. 16-bit packet IDs
3. Generation counters and rollover
4. Command versus CommandLow
5. Ack versus AckLow
6. Selective-repeat receive window
7. Out-of-order packets
8. Duplicate packets and duplicate ACKs
9. Outgoing retransmission queue
10. Retransmission timers
11. Fragment ordering
12. Interaction with session crypto
13. Bootstrap sequence starting values

Related chapters: [Packet format](packet_format.md), [Handshake](handshake.md), [Connection lifecycle](connection_lifecycle.md).
