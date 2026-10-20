# GTK client

`ts-gtk` is a deliberately small graphical front end for the same client runtime the terminal client uses. It offers what `ts-cli` offers, presented as a window: a live channel/client tree, one text feed per conversation, and the full set of slash commands. It runs on Linux and Windows; see [Windows](#windows).

```bash
build/linux/client-gtk/ts-gtk voice.example.net:9987
```

## Layout

From top to bottom:

1. **Audio bar**: *Microphone* (mute/unmute transmission; starts muted every session, exactly like `ts-cli`), *Speakers* (mute/unmute everyone's voice; see [Audio and voice](audio.md)), and the [audio settings](#audio-settings) button. Your nickname and the server name are shown on the right.
2. **Channel tree**: a live tree (a GTK 4 `GtkListView` over a `GtkTreeListModel`) of the server, its channels and the clients in them. It updates as people join, leave, move, rename or mute themselves. Every client has a talk indicator in front of their name: an empty circle that turns green while they transmit (you included). Your current channel and your own name are bold, a client's state is shown after their name (`[mic muted]`, `[afk]`, ...), and a blue `(n)` marks unread lines in a conversation. Right-click a client to adjust them (see [Per-user volume](#per-user-volume)).
3. **Text feed**: the conversation selected in the tree.
4. **Input line**: what you type goes to the selected conversation; lines starting with `/` are commands.

## Conversations

Every row in the tree is a conversation:

| Selected row | Feed shows | Typing sends |
| --- | --- | --- |
| the server (top row) | server chat | a server message |
| your current channel | that channel's chat and join/leave notices | a channel message |
| another channel | what was seen there while you were in it | nothing; double-click the channel to join it first |
| a client | your private conversation with them | a private message |

Until you click a row yourself, the feed follows your current channel. Local feedback (command output, errors, pokes, disconnect reasons) appears in whatever conversation is selected at the time.

## Audio settings

The settings button opens a window with:

- microphone and playback device;
- RNNoise noise suppression;
- an input level meter with a voice-detected indicator, and the activation threshold slider, so you can see where your voice and your background noise land relative to the threshold;
- **Test microphone**: plays back exactly what would be transmitted, i.e. after noise suppression, voice activation and Opus encoding/decoding. It stays audible even with the speakers muted.

While the window is open your microphone is muted on the server (the *Microphone* button is disabled meanwhile). Closing it restores the previous state. Device, RNNoise and threshold changes apply immediately and are saved to `config.conf`, like the equivalent `/audio` commands.

## Per-user volume

Right-click another client in the tree for a popover with a volume slider (-60 to +12 dB) and *Mute locally*, plus *Reset*. Changes are audible immediately and are stored in the same per-user files as `/user <client> volume|mute` (see [Configuration and identity](configuration.md#remote-user-settings)). They only affect your own playback.

## Commands

All of the terminal client's commands work in the input line (`/join`, `/nick`, `/pm`, `/r`, `/list`, `/user ...`, `/audio ...`, `/mute`, `/unmute`, `/help`, `/quit`), with the same syntax; see the [Usage guide](usage.md). `/clear` clears the selected conversation. Closing the window, `/quit`, `SIGINT` and `SIGTERM` all disconnect cleanly.

## Configuration

`ts-gtk` uses the same `config.conf`, `identity` and `users/` files as `ts-cli`, so nickname changes, audio device/filter choices and per-user volume/mute settings are shared between the two. On a first run without `config.conf` it writes the default profile instead of prompting; see [First-run setup](configuration.md#first-run-setup).

## Architecture

`client-gtk/` contains only presentation code on top of `ts-client-support` and `ts-config`:

- Configuration, identity and per-user settings come from `ts-config`, the same code `ts-cli` uses (see [Configuration and identity](configuration.md)). The only difference at startup is that a missing config is created with defaults (`ConfigSetup::CreateDefault`) instead of the terminal's interactive setup.

- `ConversationLog` and `RouteEvent` (`ts-client-gtk-support`) decide which conversation each runtime event belongs to and keep bounded per-conversation history with unread counts. They don't depend on GTK and are unit-tested without a display.
- `ChannelTree`, `FeedView`, `UserAudioPopover`, `AudioSettingsWindow` and `MainWindow` are the gtkmm-4.0 widgets.
- `NetworkRuntime` runs with `EnableLiveStateEvents()`, so the tree is rendered from `ServerSnapshotEvent` copies and `TalkStateEvent`s rather than from protocol state owned by the network thread (see [Runtime model](runtime.md)).
- Per-user changes go to the network thread as a `SetUserAudioAction`, which persists them and applies them to the mix, exactly like `/user`.
- Slash commands go through the shared `cli::CommandHandler`, with the window as its `cli::CommandOutput`.

## Windows

`make windows-cross` and `make windows-msys` build `ts-gtk.exe` as a window (GUI subsystem) program and assemble a self-contained folder, `build/<target>/dist/ts-gtk/`. Zip that folder and run `bin\ts-gtk.exe` on any 64-bit Windows machine:

```text
ts-gtk/bin/                     ts-gtk.exe and the GTK/gtkmm DLLs it needs
ts-gtk/share/icons/             Adwaita and hicolor icon themes
ts-gtk/share/glib-2.0/schemas/  compiled GTK settings schemas
```

Like everything else on Windows, `ts-gtk.exe` links its libraries dynamically and ships next to them (see [BUILDING.md](../../BUILDING.md#output)), so it and the gtkmm DLLs share one C++ runtime (`libstdc++-6.dll`). Unlike `ts-cli`'s flat folder, this one mirrors an install prefix, because GTK finds its icons and schemas relative to its own DLLs. `cmake/bundle_windows.cmake` assembles it from the executable's DLL imports.

The only Windows-specific source is `client-gtk/src/platform/win32/win32_shutdown_signal.cpp`. A GUI program has no SIGINT, so it closes the window cleanly on console control events (when started from a console) and on logoff or shutdown. Windows may end the process soon after a logoff notification, so disconnecting from the server is best effort there; the server times out the client otherwise.

Configuration lives in `%APPDATA%\ts-cli\`, shared with `ts-cli.exe`, exactly as on Linux.

To try it under Wine, use the font workaround in [Running it under Wine](../../BUILDING.md#running-it-under-wine); without it, Wine garbles GTK's text.
