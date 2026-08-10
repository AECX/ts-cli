# Usage guide

This is the full walkthrough of everyday `ts-cli` usage: navigating channels, messaging, voice, audio devices, per-user controls, and the complete command reference. For a five-minute quick start, see the main [README](../../README.md); for the underlying audio and configuration models, see [Audio and voice](audio.md) and [Configuration and identity](configuration.md).

Typing text without a leading slash sends it to the current channel.

---

## Everyday usage

### See who is online

Use `/list` to inspect channels and users.

```text
/list
```

Example:

```text
Lobby
├── Alice
├── Bob
└── MusicBot

Gaming
├── Charlie
└── Dave

AFK
└── Eve
```

You can also inspect a specific channel:

```text
/list Gaming
```

This is useful when you want to check who is around before moving.

### Join a channel

```text
/join Gaming
```

If a channel name contains spaces, quote it where required:

```text
/join "Late Night Gaming"
```

Once joined, normal text is sent to that channel:

```text
anyone still playing?
```

### Send private messages

Use either `/pm` or `/message`:

```text
/message Alice are you joining voice?
```

or:

```text
/pm Alice are you joining voice?
```

Reply to the last private message with:

```text
/r yep, give me a minute
```

This is especially convenient when you are already talking to someone and do not want to keep retyping their name.

### Change your nickname

```text
/nick frAgZ-laptop
```

The change applies to the current running client and is reflected on the server.

---

## Voice activation

`ts-cli` can automatically open and close transmission based on microphone level.

```text
/audio threshold -45
```

The threshold is expressed in **dBFS**. A lower value is more sensitive, a higher value requires louder input. A practical starting point for many microphones is `-45`.

Your microphone remains logically unmuted while voice activation decides when actual voice packets should be transmitted — that prevents the client from appearing to transmit continuously during silence. See [Audio and voice](audio.md#microphone-activation) for the full model.

## RNNoise

If `ts-cli` was compiled with RNNoise support, microphone noise filtering can be enabled with:

```text
/audio filter rnnoise
```

Disable it again with:

```text
/audio filter none
```

Check the current audio state with:

```text
/audio status
```

RNNoise is applied before voice-activity detection and Opus encoding.

---

## Audio devices

List available devices (PipeWire on Linux, WASAPI on Windows):

```text
/audio devices
```

Select the default input:

```text
/audio input default
```

Or choose a specific device by id or name:

```text
/audio input <id>
/audio input "USB Microphone"
```

Select an output device the same way:

```text
/audio output default
/audio output "USB Headset"
```

A common setup might look like:

```text
/audio input "USB Microphone"
/audio output "HD Audio Controller"
/audio filter rnnoise
/audio threshold -42
/unmute
```

See [Audio and voice](audio.md) for the full audio model and platform notes.

---

## Per-user controls

One of the more useful features of `ts-cli` is the ability to adjust remote users locally. These settings affect only your client.

### Lower a loud user

```text
/user Alice volume -8
```

Or use a percentage:

```text
/user Alice volume 70%
```

Restore the default:

```text
/user Alice volume reset
```

Example:

```text
> /user MusicBot volume -14
MusicBot volume: -14.0 dB
```

This is particularly useful for music bots, users with unusually loud microphones, or anyone whose level does not match the rest of the channel.

### Locally mute a user

```text
/user MusicBot mute
/user MusicBot unmute
```

This does not change their server permissions or mute state for anybody else.

### Inspect a user

```text
/user Alice
```

When nicknames are ambiguous, use the TeamSpeak client ID:

```text
/user #42
/user #42 volume -10
```

Persistent settings are stored using the user's stable TeamSpeak identity rather than their nickname or temporary client ID, so a saved volume adjustment continues to apply even if that person reconnects with another client ID or changes their nickname. See [Configuration and identity](configuration.md#remote-user-settings) for the file layout.

---

## Useful scenarios

### Stay inside tmux or zellij

A terminal-native voice client fits naturally beside shells, editors, build output, logs, and monitoring tools:

```text
┌───────────────────────────────┬──────────────────────┐
│                               │                      │
│            nvim               │      build logs      │
│                               │                      │
├───────────────────────────────┼──────────────────────┤
│                               │                      │
│           shell               │       ts-cli         │
│                               │                      │
└───────────────────────────────┴──────────────────────┘
```

No extra desktop window is required just to stay in voice.

### Check a server quickly

Sometimes you do not need to talk at all:

```bash
build/linux/client/ts-cli voice.example.net:9987
```

```text
/list
```

You can quickly check whether people are online, what channels are active, and whether it is worth joining.

### Keep a music bot under control

Suppose a music bot joins every evening and is consistently too loud. Set it once:

```text
/user MusicBot volume -12
```

The preference is persisted using its TeamSpeak identity, so the next time that same user connects, the adjustment remains available without relying on its current client ID.

### Switch from speakers to a headset

You can change playback devices without restarting the client:

```text
/audio devices
/audio output "USB Headset"
/audio input "USB Headset Microphone"
/audio status
```

### Tune voice activation

Start with:

```text
/audio threshold -45
```

If keyboard noise triggers transmission, raise it (`-38`); if your voice is being cut off, lower it (`-50`). If RNNoise is available, combine both:

```text
/audio filter rnnoise
/audio threshold -45
```

---

## Command reference

### Messaging

```text
/pm <client> <text>
/message <client> <text>
/r <text>
```

Examples:

```text
/message Alice ping me when you're ready
/pm Bob check your microphone
/r yep, works now
```

### Channels

```text
/join <channel>
/list [channel]
```

Examples:

```text
/join Lobby
/join "Late Night Gaming"
/list
/list Lobby
```

### Nickname

```text
/nick <new name>
```

Example:

```text
/nick frAgZ-workstation
```

### Per-user controls

```text
/user <client>
/user <client> volume <dB|percent|reset>
/user <client> mute
/user <client> unmute
```

Examples:

```text
/user Alice
/user Alice volume -6
/user Bob volume 80%
/user MusicBot mute
/user #42 volume reset
```

### Microphone

```text
/mute
/unmute
```

### Audio

```text
/audio status
/audio devices
/audio input <default|id|name>
/audio output <default|id|name>
/audio filter
/audio filter rnnoise
/audio filter none
/audio threshold <dBFS>
```

### Client

```text
/help
/quit
```
