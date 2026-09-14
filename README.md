![ts-cli logo](assets/logo.png)

# ts-cli

[![License: GPL v3](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](CONTRIBUTING.md)
[![Platforms](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)](BUILDING.md)

**A native TeamSpeak voice client for the terminal, written in C++20.**

`ts-cli` is a terminal-first TeamSpeak client, built natively for Linux and cross-compiled to Windows. It connects directly using the TeamSpeak client protocol and implements the connection, session crypto, command transport, channel/client state, messaging, and voice path natively.

It does **not** use the proprietary TeamSpeak SDK, and it is **not** a ServerQuery wrapper.

<p align="center">
  <img src="assets/ping/greeting.png" alt="ts-cli mascot waving" width="260">
</p>

> [!NOTE]
> `ts-cli` is under active development. The goal is not to reproduce every feature of the desktop client, but to provide a small, capable, understandable terminal client for everyday TeamSpeak use.

> [!TIP]
> Windows support is newer than Linux, but navigation, chat, configuration, and voice (via WASAPI) all work on real hardware. A few audio features are still missing there — exclusive mode and live device hot-plug notifications — see [BUILDING.md](BUILDING.md) for the full picture and known limitations.

## What you can do

With `ts-cli`, you can:

* connect directly to TeamSpeak servers;
* browse channels and connected users, join and move between channels;
* talk and listen (PipeWire on Linux, WASAPI on Windows), with configurable voice activation and optional RNNoise filtering;
* send channel messages, private messages, and reply to the last PM;
* change your nickname at runtime, select input/output devices, mute/unmute;
* adjust or locally mute individual remote users without affecting anyone else;
* keep useful preferences across sessions in standard XDG locations.

The client starts every session with the microphone muted.

---

## Quick start

> This assumes your build dependencies are already installed. First time here? Set up your environment with [BUILDING.md](BUILDING.md) — it covers both Linux and Windows.

```bash
make
build/linux/client/ts-cli voice.example.net:9987
```

Once connected, type normally to send a message to the current channel, or use a command:

```text
hello everyone
/list
/join Gaming
/unmute
/quit
```

<p align="center">
  <img src="assets/ping/laptop.png" alt="ts-cli mascot using the terminal" width="280">
</p>

### Example session

```text
$ build/linux/client/ts-cli voice.example.net:9987

Connected as frAgZ

Lobby
├── Alice
├── Bob
└── MusicBot

> hello everyone
[Lobby] frAgZ > hello everyone

> /join Gaming
Joined channel: Gaming

> anyone up for a game?
[Gaming] frAgZ > anyone up for a game?

> /unmute
microphone unmuted
```

For the full walkthrough — channels, messaging, voice activation, audio devices, per-user controls, and the complete command reference — see the **[Usage guide](docs/client/usage.md)**.

---

## Voice

`ts-cli` supports both TeamSpeak **Opus Voice** and **Opus Music** channels over a native audio path:

```text
capture -> optional filter -> voice-activity detection -> Opus encode -> network
network -> Opus decode -> per-user volume/mute -> mix -> playback
```

The microphone starts muted every session; unmute with `/unmute`, mute again with `/mute`. See [Audio and voice](docs/client/audio.md) for the full pipeline and [Usage guide](docs/client/usage.md) for tuning voice activation, RNNoise, and devices.

<p align="center">
  <img src="assets/ping/talking.png" alt="ts-cli mascot using voice chat" width="250">
</p>

---

## Configuration

`ts-cli` stores its state in the standard XDG configuration directory:

```text
$XDG_CONFIG_HOME/ts-cli/    (~/.config/ts-cli/ if unset)
├── config.conf     # client-wide behavior and preferences
├── identity        # local TeamSpeak identity
└── users/          # per-remote-user local settings, keyed by stable identity
```

See [Configuration and identity](docs/client/configuration.md) for the full layout, file formats, and migration behavior.

---

## Building

`ts-cli` builds natively on Linux and cross-compiles to Windows via MinGW-w64 (tested under Wine); it can also be built natively on Windows via MSYS2. Full environment setup and dependency lists live in **[BUILDING.md](BUILDING.md)**.

Once dependencies are installed, a single `Makefile` drives all three targets — `make linux` (default), `make windows-cross`, and `make windows-msys`:

```bash
make linux
```

The resulting binary is `build/linux/client/ts-cli`.

---

## Running a pre-built Linux binary

`ts-cli` on Linux links against its dependencies dynamically, so running a binary you didn't build yourself (e.g. downloaded from a release) needs these shared libraries installed: OpenSSL 3.x, libsodium, Opus, and RNNoise. You also need PipeWire actually running as your system's audio server, not just its library installed — see [Audio and voice](docs/client/audio.md).

#### Arch Linux

```bash
sudo pacman -S --needed openssl libsodium opus rnnoise pipewire
```

#### Fedora

```bash
sudo dnf install openssl-libs libsodium opus rnnoise pipewire
```

#### Debian / Ubuntu

```bash
sudo apt install libssl3 libsodium23 libopus0 librnnoise0 libpipewire-0.3-0
```

> [!TIP]
> Debian/Ubuntu package names occasionally change between releases (for example, a `t64` suffix on newer releases from the 64-bit `time_t` transition — `libssl3t64`, `libpipewire-0.3-0t64`). If a package above isn't found, run `ldd path/to/ts-cli` to see exactly which `.so` is missing, then search for it with your package manager: `pacman -F <name>.so` on Arch, `dnf provides '*/<name>.so'` on Fedora, or `apt search <library-name>` on Debian/Ubuntu.

---

## How it works

The project implements the TeamSpeak client path itself rather than delegating it to the proprietary SDK: native UDP connection and handshake, session encryption, packet sequencing and reliable command transport, QuickLZ decompression, identity handling, live channel/client state, Opus voice transmit/receive with jitter handling, native audio capture/playback and mixing, and persistent configuration.

For protocol and architecture details, see the documentation under [`docs/`](docs/README.md).

## Project structure

```text
ts-cli/
├── audio/       # PipeWire/WASAPI, Opus, filters, jitter, mixing and audio worker
├── client/      # CLI, configuration, persistence and runtime integration
├── docs/        # architecture and protocol documentation
├── log/         # logging
├── net/         # networking
└── protocol/    # TeamSpeak protocol, crypto, session state and transport
```

The project keeps these boundaries intentionally strict: protocol code does not own UI or filesystem state, audio code does not need to understand TeamSpeak command parsing, and the CLI remains a relatively thin user-facing layer over the underlying runtime and state APIs.

<p align="center">
  <img src="assets/ping/presenting.png" alt="ts-cli mascot presenting commands" width="250">
</p>

---

## Documentation

* [Documentation index](docs/README.md)
* [Usage guide](docs/client/usage.md) — everyday commands and full command reference
* [Using the protocol library](docs/protocol/api.md) — the C++ API, with worked examples, for writing your own client against `ts-protocol`
* [Audio and voice](docs/client/audio.md)
* [Runtime model](docs/client/runtime.md)
* [Configuration and identity](docs/client/configuration.md)
* [Building ts-cli](BUILDING.md) — environment setup for Linux and Windows
* [Contributing](CONTRIBUTING.md) — style, architecture and project conventions
* [Protocol documentation](docs/protocol/README.md) — handshake, packet format, reliability, commands, crypto, state and voice

---

## Current scope

`ts-cli` is intended to be useful for normal voice and chat interaction, but it deliberately does not attempt complete desktop-client parity. Areas that remain smaller or incomplete include advanced administration/server-management workflows, desktop-style UI features and overlays, a fully adaptive heavyweight jitter engine, arbitrary microphone filter chains beyond the current optional RNNoise stage, and exclusive-mode audio / live device hot-plug notifications on Windows (see [BUILDING.md](BUILDING.md#current-limitations)).

Keeping the scope focused is intentional: the goal is a compact native terminal client that remains understandable, hackable, and pleasant to use.

## Development

Build environment setup is documented in [BUILDING.md](BUILDING.md); development and contribution guidelines are documented in [CONTRIBUTING.md](CONTRIBUTING.md). Protocol changes should remain focused, tested, and contained within the protocol layer where possible. Audio, client runtime, and persistence changes should preserve the existing ownership boundaries between components.

---

## Disclaimers

This project is not affiliated with or endorsed by TeamSpeak Systems GmbH. TeamSpeak and related trademarks belong to their respective owners.

AI assistance was used in this project to help with research, draft designs, and fill in documentation. The application itself is, and is intended to remain, handcrafted.
