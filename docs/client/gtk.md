# GTK client

`ts-gtk` is a deliberately small graphical front end for the same client runtime the terminal client uses. It presents the session as a window: a live channel/client tree, one text feed per conversation, and the audio and identity options. It has no slash commands; everything is done through the window. It runs on Linux and Windows; see [Windows](#windows).

```bash
build/linux/client-gtk/ts-gtk                         # start disconnected
build/linux/client-gtk/ts-gtk voice.example.net:9987  # connect right away
```

## Connecting

Started without an address, the window is empty and disconnected. The server address field in the top bar is prefilled with the last server you connected to (`last_server` in `config.conf`); press Enter or *Connect* to connect. The button follows the connection:

| State | Button | Address field |
| --- | --- | --- |
| disconnected | *Connect* | editable |
| connecting | *Cancel* | locked |
| connected | *Disconnect* | locked |

Connecting runs in the background, so the window stays responsive. Cancelling takes effect once the attempt finishes or times out; an attempt that succeeds after being cancelled is disconnected right away. Failures and disconnects (by you, the server, or a timeout) are reported in the feed. After a disconnect the tree is cleared, but the conversation stays readable until you connect again, which starts with empty conversations. Each connection uses the nickname and identity saved at that moment and, like `ts-cli`, starts with the microphone muted.

## Layout

From top to bottom:

1. **Top bar**: *Microphone* (mute/unmute transmission; starts muted every session, exactly like `ts-cli`, and is only available while connected), *Speakers* (mute/unmute everyone's voice; see [Audio and voice](audio.md)), [*Options*](#options), and the server address with the [connect button](#connecting). Your nickname and the server name are shown in the window title.
2. **Channel tree**: a live tree (a GTK 4 `GtkListView` over a `GtkTreeListModel`) of the server, its channels and the clients in them. It updates as people join, leave, move, rename or mute themselves. Every client has a talk indicator in front of their name: an empty circle that turns green while they transmit (you included). The circle is filled red for clients you muted locally. Your current channel and your own name are bold, a client's state is shown after their name (`[mic muted]`, `[afk]`, `[locally muted]`, ...), and a blue `(n)` marks unread lines in a conversation. Right-click a client to adjust them (see [Per-user volume](#per-user-volume)).
3. **Text feed**: the conversation selected in the tree.
4. **Input line**: what you type goes to the selected conversation as chat, including lines starting with `/`.

## Conversations

Every row in the tree is a conversation:

| Selected row | Feed shows | Typing sends |
| --- | --- | --- |
| the server (top row) | server chat | a server message |
| your current channel | that channel's chat and join/leave notices | a channel message |
| another channel | what was seen there while you were in it | nothing; double-click the channel to join it first |
| a client | your private conversation with them | a private message |

Until you click a row yourself, the feed follows your current channel. Local feedback (errors, pokes, disconnect reasons) appears in whatever conversation is selected at the time.

## Options

*Options* opens a window with two tabs, *Audio* and *Identity*. It is modal: the main window can't be used until you close it.

### Audio

The *Audio* tab has:

- microphone and playback device;
- RNNoise noise suppression;
- an input level meter with a voice-detected indicator, and the activation threshold slider, so you can see where your voice and your background noise land relative to the threshold;
- the device clock: the sample rate and period each device runs at, and whether it is resampled (see [Sample rate and device clock](audio.md#sample-rate-and-device-clock));
- **Test microphone**: plays back exactly what would be transmitted, i.e. after noise suppression, voice activation and Opus encoding/decoding. It stays audible even with the speakers muted.

While *Test microphone* is on, your microphone is muted on the server; turning it off or closing the options restores the previous state. The *Microphone* button is disabled while the options are open. Device, RNNoise and threshold changes apply immediately and are saved to `config.conf`, like `ts-cli`'s `/audio` commands.

### Identity

The *Identity* tab has:

- **Nickname**: saved to `config.conf` with *Save* or Enter. While connected, it also renames you on the server right away, like `/nick`.
- **Identity**: the unique ID other clients and servers know you by, plus the security level and key offset of the one local `identity` file (see [Configuration and identity](configuration.md#local-identity)).
- **Raise security level to** *n* / **Increase**: searches for a key offset reaching a higher [security level](configuration.md#raising-the-security-level), which some servers require. It runs in the background and shows its progress; *Stop*, or closing the options, ends it without changing the identity. Your unique ID stays the same.
- **Generate new…** replaces the identity with a new one at the default security level; **Import PEM…** replaces it with a private key from a PEM file, computing its key offset. Both ask first: servers treat a new identity as a new user, without your previous groups and permissions.
- **Export PEM…** saves the private key as a PEM file (owner-only), e.g. as a backup or to use it in another client. Keep it private; it is your login.

Apart from the nickname, changes here apply from the next connection on.

## Per-user volume

Right-click another client in the tree for a popover with a volume slider (-60 to +12 dB) and *Mute locally*, plus *Reset*. Changes are audible immediately and are stored in the same per-user files as `/user <client> volume|mute` (see [Configuration and identity](configuration.md#remote-user-settings)). They only affect your own playback.

## Exiting

Closing the window, `SIGINT` and `SIGTERM` all disconnect cleanly and exit.

## Configuration

`ts-gtk` uses the same `config.conf`, `identity` and `users/` files as `ts-cli`, so nickname changes, identity changes, audio device/filter choices and per-user volume/mute settings are shared between the two. It additionally remembers `last_server`. On a first run without `config.conf` it writes the default profile instead of prompting; see [First-run setup](configuration.md#first-run-setup).

## Architecture

`client-gtk/` contains only presentation code on top of `ts-client-runtime` and `ts-config`. It doesn't link `ts-client-support`, the terminal client's commands and terminal output:

- Configuration, identity and per-user settings come from `ts-config`, the same code `ts-cli` uses (see [Configuration and identity](configuration.md)). The only difference at startup is that a missing config is created with defaults (`ConfigSetup::CreateDefault`) instead of the terminal's interactive setup.

- `ConversationLog` and `RouteEvent` (`ts-client-gtk-support`) decide which conversation each runtime event belongs to and keep bounded per-conversation history with unread counts. They don't depend on GTK and are unit-tested without a display.
- `ChannelTree`, `FeedView`, `UserAudioPopover`, `OptionsWindow` (with `AudioSettingsPage` and `IdentityPage`) and `MainWindow` are the gtkmm-4.0 widgets. Each tree row (`ChannelTree::Item`) keeps the properties it shows, e.g. a client and whether they are locally muted, and builds its label from them (`Item::Markup()`).
- The configuration, audio engine and window live as long as the program. Each connection is a `Session`: it connects on a background thread, then owns the `Connection`, its action/event queues and the `NetworkRuntime`, and is discarded when the connection ends. The audio engine wakes whichever session's connection is current through `NetworkWake`.
- `NetworkRuntime` runs with `EnableLiveStateEvents()`, so the tree is rendered from `ServerSnapshotEvent` copies and `TalkStateEvent`s rather than from protocol state owned by the network thread (see [Runtime model](runtime.md)).
- Per-user changes go to the network thread as a `SetUserAudioAction`, which persists them and applies them to the mix, exactly like `ts-cli`'s `/user`. Each snapshot lists the clients currently muted in the mix (`locallyMutedClientIds`), so the tree follows mutes from either path.

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
