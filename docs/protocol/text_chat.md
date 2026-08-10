# Text chat

> Status: outline, except for the Poke section below

This chapter will document the subset of TeamSpeak text messaging intended for `ts-cli`.

## Planned sections

1. Channel messages
2. Server messages
3. Private client messages
4. Outgoing text-message command
5. Incoming text-message notification
6. Sender and target identification
7. Command escaping
8. Interaction with client/channel state
9. Terminal presentation

Administrative messaging features are outside the initial scope.

## Poke

A poke ("ping" another user) is TeamSpeak's `notifyclientpoke` notification:
a short, attention-grabbing one-off message, distinct from normal channel or
private chat. Unlike `notifytextmessage`, a poke command carries exactly one
poke per command (`protocol::ClientPoke::Parse` requires exactly one row)
with four fields — `invokerid`, `invokername`, `invokeruid`, `msg` — parsed
the same way as `notifytextmessage`'s invoker fields (see
[Command protocol](commands.md) once written).

Real TeamSpeak servers send pokes over the low-priority `CommandLow` stream
rather than `Command` (see
[Command versus CommandLow](reliability.md#command-versus-commandlow)), so
it arrives independently of, and is not held up behind, ordinary chat/state
traffic on `Command`.

A poke has no application-level reply — the client only needs to
acknowledge it at the transport layer (an `AckLow`, sent automatically by
`SessionTransport` for every accepted `CommandLow` packet). `ts-cli` has no
`/poke`-style outgoing command; this is a receive-only notification.

On receipt, `Session::ProcessCommand` turns it into a `protocol::PokeEvent`,
which flows through the existing `SessionEvent` → `RuntimeEvent` →
`Presentation` pipeline (see [Runtime model](../client/runtime.md)) like any
other event. `Presentation::PrintPoke` renders it as `[poke] <name>:
<message>` in a distinct red/italic style — different from the
Magenta/Green/Yellow used for private/channel/server chat and from the
plain red used for errors — and plays a dedicated notification sound
(`ts::audio::NotificationType::Reminder`), so a poke reads unambiguously as
a poke rather than a normal chat message.
