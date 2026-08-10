# Reliability and sequencing

> Status: outline, except for the Command/CommandLow and Ack/AckLow sections below

TeamSpeak builds reliable ordered command delivery on top of UDP.

## Command versus CommandLow

`Command` and `CommandLow` are two independent reliable, ordered, fragmenting
command streams, each with its own 16-bit packet ID / 32-bit generation
sequence (tracked separately in `PacketSequenceState`, one slot per type) and
its own out-of-order receive window and fragment-reassembly buffer
(`protocol::CommandReceiveWindow`, one instance per stream in
`SessionTransport`). A command sent on one stream has no ordering
relationship with commands on the other; only packets within the same stream
are guaranteed to arrive, and be delivered upward, in order.

`CommandLow` exists so lower-priority notifications are not held up behind
bulk traffic on the `Command` stream. The clearest example in `ts-cli` today
is the poke notification (`notifyclientpoke`, see
[Text chat](text_chat.md#poke)), which real TeamSpeak servers send over
`CommandLow` specifically so it is not queued behind, say, a large
`channellist` still being delivered on `Command`.

`ts-cli` currently only receives `CommandLow`; it has no need to originate
`CommandLow` traffic, so `SendCommand` always uses `Command`.

## Ack versus AckLow

Every `Command` packet is acknowledged with an `Ack`, and every `CommandLow`
packet with an `AckLow` — same wire shape (the acknowledged packet's ID),
different packet type, matching which stream the acknowledged packet came
in on. `SessionTransport::SendAck` takes the ack type as a parameter for
exactly this reason. An ack is sent for every accepted packet, including
duplicates/old packets, so the sender's retransmission timer clears
promptly even if a previous ack was lost.

Outgoing reliability tracking (retransmission timers, RTT sampling) exists
today only for the `Command` stream (`ReliableCommandQueue`, matched against
incoming `Ack`), since `ts-cli` never sends `CommandLow`. `HandleAckLow`
still validates and sequences incoming `AckLow` packets so a stray one never
crashes the session; it just has nothing to acknowledge yet.

## Planned sections

1. Per-packet-type sequence state
2. 16-bit packet IDs
3. Generation counters and rollover
4. Selective-repeat receive window
5. Out-of-order packets
6. Duplicate packets and duplicate ACKs
7. Outgoing retransmission queue
8. Retransmission timers
9. Fragment ordering
10. Interaction with session crypto
11. Bootstrap sequence starting values

Related chapters: [Packet format](packet_format.md), [Handshake](handshake.md), [Connection lifecycle](connection_lifecycle.md).
