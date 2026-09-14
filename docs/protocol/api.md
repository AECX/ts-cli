# Using the protocol library

The other chapters in this directory describe what goes over the wire. This one describes how
to drive that from C++: the `ts-protocol` library's public API, as used by `ts-cli` itself and
by any other program that links against it.

Everything here lives in namespace `ts::protocol` and is reachable through one header:

```cpp
#include <protocol/protocol.hpp>
```

The individual headers remain available if you prefer narrower includes; `protocol/protocol.hpp`
is simply the whole public surface in one place.

## Layers

```text
Connection        establish, drive, and shut down a session; the API you want
  Session         protocol state machine: login, commands, channel/client stores, events
    SessionTransport  packet sequencing, reliability, ACKs, ping/pong, fragmentation
      Transport       datagram send/receive over a UDP socket
```

`Connection` owns the layers beneath it. Reach past it only if you are working on the protocol
itself — `Session` and below assume a caller that already got every detail right, and none of
them are safe to touch from a second thread.

## A complete program

This connects, prints the channel tree, echoes every text message it sees, and leaves cleanly
on Ctrl-C. It is the whole API in miniature.

```cpp
#include <atomic>
#include <csignal>
#include <fstream>
#include <iostream>
#include <protocol/protocol.hpp>
#include <sstream>
#include <stop_token>
#include <string>

namespace {

    std::stop_source Stopping;

    void HandleSignal( int ) {
        Stopping.request_stop();
    }

    std::string ReadFile( const char* path ) {
        std::ifstream stream( path );
        std::ostringstream contents;

        contents << stream.rdbuf();

        return contents.str();
    }

} // namespace

int main( int argc, char* argv[] ) {
    if ( argc != 3 ) {
        std::cerr << "usage: example <host:port> <identity.pem>" << std::endl;

        return 1;
    }

    std::signal( SIGINT, HandleSignal );

    try {
        ts::protocol::ClientProfile profile;
        profile.nickname = "example";
        profile.version.initVersion = 0x142898dd;
        profile.version.version = "3.6.2 [Build: 1695203294]";
        profile.version.platform = "Linux";
        profile.version.signature = "p4iF1jZ3ZOz9MEkKSZ2bvnFtm9WmUcQy9mAP//erFE4PF1sB6K1CSANrr+3X4B0aZR0u+K2pjnv8kiKsWKQCBQ==";

        ts::protocol::Connection connection( ts::protocol::ConnectionOptions {
            .endpoint = argv[1],
            .profile = std::move( profile ),
            .identity = ts::protocol::Identity::FromPrivateKeyPem( ReadFile( argv[2] ) ),
            .identityKeyOffset = 0 } );

        connection.Connect();

        std::cout << "connected to " << connection.ServerName() << " as client " << connection.ClientId() << std::endl;

        for ( const ts::protocol::ChannelTreeEntry& entry : connection.Channels().Tree() ) {
            std::cout << std::string( entry.depth * 2, ' ' ) << entry.channel->name << std::endl;
        }

        const ts::protocol::DisconnectInfo info =
            connection.Run( Stopping.get_token(), []( ts::protocol::SessionEvent event ) {
                ts::protocol::Visit(
                    event,
                    []( const ts::protocol::TextMessageEvent& message ) {
                        std::cout << message.message.invokerName << ": " << message.message.text << std::endl;
                    },
                    []( const auto& ) {} );
            } );

        if ( info.reason != ts::protocol::DisconnectReason::LocalRequest ) {
            std::cerr << "session ended: " << info.message << std::endl;

            return 1;
        }
    } catch ( const std::exception& exception ) {
        std::cerr << "error: " << exception.what() << std::endl;

        return 1;
    }

    return 0;
}
```

Link it against `ts-protocol`; the library brings its own dependencies:

```cmake
target_link_libraries(example PRIVATE ts-protocol)
```

## Connecting

`ConnectionOptions` carries everything needed to establish a session. It is move-only, because
`Identity` owns a private key that is deliberately not copyable.

```cpp
struct ConnectionOptions {
    std::string endpoint;
    ClientProfile profile;
    Identity identity;
    std::uint64_t identityKeyOffset = 0;
};
```

`endpoint` accepts `host`, `host:port` or a bare address, and is resolved — including SRV
lookup — when the `Connection` is constructed. Construction can therefore fail on a bad name
before you ever call `Connect`.

**`identity`** is your TeamSpeak identity: a P-256 private key that gives you a stable unique
id on every server. Generate one once and keep it; the server has no idea who you are without
it.

```cpp
Identity identity = Identity::FromPrivateKeyPem( pem );

const std::string pem = identity.PrivateKeyPem();   // persist this
```

**`identityKeyOffset`** is that identity's hashcash counter. Servers can require a minimum
identity security level, and the offset is the proof-of-work that raises it. It belongs to the
identity, so store the two together — `ts-cli` keeps both in one file, see
[Configuration and identity](../client/configuration.md).

**`profile.version`** must describe a real TeamSpeak client release: `initVersion`, `version`,
`platform` and a matching Base64 `signature`. The server verifies the signature during the
handshake and refuses mismatched combinations, so these four fields travel as a set — you
cannot change the platform string and keep the old signature. `ts-cli`'s known-good defaults
are in `Config::DefaultProfile()` (`client/src/config/config.cpp`).

`Connect()` runs the handshake, logs in, and blocks until the server has delivered the initial
channel and client lists. When it returns, the session is `Established` and everything below
is safe to call.

## Driving the session

All session state lives on one thread: whichever thread calls `Run` or `Poll`. Pick one of the
two styles below — not both.

### Run: hand over the thread

```cpp
const DisconnectInfo info = connection.Run(
    stopToken,
    []( SessionEvent event ) { /* one event */ },
    [] { /* once per iteration, before packets are pumped */ } );
```

Both handlers execute on the owner thread. The cycle handler is where you drain your own work
queue: it is the only place another component's requests can safely reach the `Connection`.
`ts-cli` uses exactly this shape — see [Runtime model](../client/runtime.md).

`Run` returns when the stop token is triggered, the server drops you, or nothing is received
for 30 seconds. It does not propagate exceptions from the session loop; they come back as
`DisconnectReason::TransportError`. An exception thrown by *your* handler is your problem and
does propagate.

### Poll: keep your own loop

```cpp
while ( running ) {
    connection.Poll( std::chrono::milliseconds { 50 } );

    while ( connection.HasEvent() ) {
        Handle( connection.TakeEvent() );
    }
}

connection.Disconnect( "Done" );
```

`Poll` runs timers, waits up to the timeout for a packet, processes it, and returns whether one
arrived. It never blocks longer than the timeout, so a modest value keeps the loop responsive.
Events accumulate until you drain them.

### Waking the loop from another thread

`Wake()` is the *only* member that is safe to call from another thread. It interrupts a
blocked `Poll` so the owner thread notices work you queued for it:

```cpp
actions.Push( action );   // your queue, your locking
connection.Wake();        // thread-safe
```

Everything else — sending, reading state, polling — must happen on the owner thread.

## Handling events

`SessionEvent` is a `std::variant`. `Visit` dispatches it without the usual boilerplate:

```cpp
Visit(
    event,
    []( const TextMessageEvent& value ) {
        // value.message.text, .invokerName, .invokerUniqueId
        // value.channelName, value.privatePeerName
        // value.replyTarget  -- pass straight back to SendTextMessage
        // value.outgoing     -- true for your own message, echoed back
    },
    []( const ClientPresenceEvent& value ) {
        // value.kind is Joined or Left, relative to *your* current channel
        // value.clientId, .clientName, .channelId, .channelName
    },
    []( const CommandErrorEvent& value ) {
        // the server rejected something you sent; value.id, value.message
    },
    []( const VoiceEvent& value ) {
        // value.frame.data is still Opus-encoded -- see below
    },
    []( const PokeEvent& value ) {
        // value.invokerName, value.message
    } );
```

Leaving an alternative unhandled is a compile error, which is what tells you when a new event
kind appears. Add a generic `[]( const auto& ) {}` last to opt out of that deliberately.

`VoiceEvent` carries the encoded payload rather than PCM: decoding belongs next to your jitter
buffering and playback clock, not in the network loop. `frame.talkStart` and `frame.talkEnd`
bracket each talk burst, and `frame.voiceId` is the per-talker sequence number you reorder on.
[Voice](voice.md) covers the wire format; [Audio and voice](../client/audio.md) covers what
`ts-cli` does with it.

## Reading server state

Two stores track what the server has told you. Both are snapshots of live state — read them on
the owner thread, and do not hold the returned pointers across a `Poll`.

```cpp
const ChannelStore& channels = connection.Channels();
const ClientStore& clients = connection.Clients();
```

`Tree()` flattens the channel hierarchy into display order with a depth for each entry:

```cpp
for ( const ChannelTreeEntry& entry : channels.Tree() ) {
    std::cout << std::string( entry.depth * 2, ' ' ) << entry.channel->name << std::endl;

    for ( const Client* client : clients.InChannel( entry.channel->id ) ) {
        std::cout << std::string( entry.depth * 2 + 2, ' ' ) << client->nickname << std::endl;
    }
}
```

Two things regularly catch people out:

- **`Channel::orderAfterId` is the id of the previous sibling, not an array index.** Zero means
  "first child under this parent". `Tree()` already resolves it; you only need this if you sort
  channels yourself.
- **`Client::detailsKnown` can be false.** A move notification may arrive before the matching
  enter-view, leaving a client whose channel placement is known but whose nickname and unique
  id are not yet. Check the flag before trusting `nickname` or `uniqueId`.

`ClientStore::All()`, `Find( clientId )` and `ChannelStore::Find( channelId )` cover direct
lookups; `Find` returns `nullptr` when the id is unknown.

## Sending

### Text

```cpp
connection.SendTextMessage( TextMessageTarget { .mode = TextMessageTargetMode::Channel,
                                                .id = connection.CurrentChannelId() },
                            "hello everyone" );

connection.SendTextMessage( TextMessageTarget { .mode = TextMessageTargetMode::Private, .id = clientId },
                            "just to you" );
```

`Server` is the third mode and broadcasts to everyone, permissions allowing. To answer a
message, reuse the `replyTarget` the event handed you rather than rebuilding it.

### Channel and nickname

```cpp
connection.MoveToChannel( channelId );
connection.ChangeNickname( "new name" );
```

Both are requests, not local mutations. The server is the source of truth: your state changes
when the resulting notification arrives, which is also when `CurrentChannelId()` starts
returning the new value. A rejected move produces a `CommandErrorEvent`.

### Audio state

```cpp
connection.SetAudioState( AudioState { .inputHardware = true, .outputHardware = true, .inputMuted = false } );
```

This is presentation only — it drives the muted/no-hardware icons other clients render. It does
not gate transmission; you control that by not calling `SendVoice`.

### Voice and whisper

```cpp
connection.SendVoice( encodedOpusFrame, talkStart );
```

`data` must be a single encoded frame matching the current channel's codec (`OpusVoice` or
`OpusMusic`); the library reads the codec from the channel and refuses non-Opus channels. Set
`talkStart` on the first frame of a burst. Send one empty frame to mark the end of a burst.

Whispers reach people outside your channel:

```cpp
connection.SendWhisper( WhisperTarget { .channelIds = { channelId }, .clientIds = { clientId } },
                        encodedOpusFrame,
                        talkStart );

connection.SendGroupWhisper( GroupWhisper { .type = GroupWhisperType::ChannelCommander,
                                            .target = GroupWhisperTarget::CurrentChannel,
                                            .targetId = 0 },
                             encodedOpusFrame,
                             talkStart );
```

A `WhisperTarget` with neither channels nor clients is rejected. Whispering usually needs a
permission the server grants explicitly.

## Statistics

```cpp
const ConnectionStatistics::Snapshot statistics = connection.Statistics();

std::cout << statistics.pingMs << " ms +/- " << statistics.pingDeviationMs << std::endl;
std::cout << statistics.packetLossTotal * 100.0 << "% loss" << std::endl;
std::cout << statistics.receivedSpeech.bandwidthLastSecond << " B/s speech in" << std::endl;
```

Ping comes from the round trip of the once-a-second keepalive. Traffic is counted separately
for speech, keepalive and control, each as `packets`, `bytes`, and bandwidth averaged over the
last second and the last minute. Loss is a fraction in `[0, 1]`, per class and in total.

The session reports the same numbers back to the server, which is how the desktop client shows
your connection quality to others.

## Disconnecting

Leaving deliberately:

```cpp
connection.Disconnect( "Back later" );
```

This sends the disconnect command and waits briefly for the server to acknowledge it, so the
reason actually arrives. The `Connection` is `Closed` afterwards and cannot be reused.

`Run` does this for you when the stop token fires. When the session ends for any other reason,
the returned `DisconnectInfo` says which:

| `reason` | Meaning |
| --- | --- |
| `LocalRequest` | Your stop token, or your `Disconnect` call. |
| `ServerClosed` | The server removed you: kick, ban, or shutdown. `serverReasonId` is the TeamSpeak reason id; `message` is the server's text, frequently empty. |
| `Timeout` | Nothing at all was received for 30 seconds. |
| `TransportError` | An exception escaped the session loop; `message` is its `what()`. |

Do not assume `message` is non-empty — a kick with no reason given is ordinary.

## Errors

Every failure the public API raises derives from `ProtocolError`, which derives from
`std::runtime_error`. Catching `std::exception` keeps working; catching `ProtocolError` lets you
separate protocol failures from everything else.

| Type | Raised when |
| --- | --- |
| `NotConnectedError` | An operation needing an established session ran before `Connect` succeeded, or after the session closed. |
| `LoginRejectedError` | The server refused the login. `Id()` is the TeamSpeak error id — a wrong password and a duplicate nickname are different ids. |
| `ProtocolError` | Everything else at this boundary, including malformed arguments. |

State accessors — `ClientId`, `Channels`, `Statistics` and the rest — throw `NotConnectedError`
rather than returning a placeholder, so a missing `Connect` fails loudly instead of looking like
an empty server. `State()` and `IsEstablished()` never throw and are the way to ask without a
try block:

```cpp
if ( connection.IsEstablished() ) {
    std::cout << connection.ServerName() << std::endl;
}
```

Failures below this boundary — a bad MAC, an oversized packet, a malformed command — still
surface as plain `std::runtime_error`. Inside `Run` they never reach you directly; they arrive
as `DisconnectReason::TransportError`.

## Threading, in one place

- One thread owns the `Connection` for its whole lifetime. That thread calls `Connect`, `Run`
  or `Poll`, every `Send*`, and every state accessor.
- `Wake()` is the only member callable from another thread.
- Both `Run` handlers execute on the owner thread; treat them as part of it.
- Move work across threads with queues, not by sharing the `Connection`.

This is not a lock that is missing — packet ids, generations, retransmission state and the
channel/client stores are all designed around serialized access. [Runtime model](../client/runtime.md)
shows the queue arrangement `ts-cli` uses.

Related chapters: [Connection lifecycle](connection_lifecycle.md),
[Server state](state.md), [Text chat](text_chat.md), [Voice](voice.md),
[Runtime model](../client/runtime.md).
