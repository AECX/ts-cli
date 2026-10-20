# Contributing to ts-cli

`ts-cli` is an experimental TeamSpeak client written in C++20, targeting Linux natively, Windows via MinGW-w64 cross-compilation, and Windows natively via MSYS2's UCRT64 environment. Keep changes small, testable, and focused on the client features the project actually intends to support.

## Build and test

Full environment setup for all three targets (Linux native, Windows cross-compiled via MinGW-w64, Windows native via MSYS2) is documented in [BUILDING.md](BUILDING.md).

Before submitting changes, run:

```bash
make linux
```

and, if you touched anything below the CLI layer (`net`, `protocol`, `log`, `audio`), also cross-compile and test the Windows target:

```bash
make windows-cross
```

Changes should compile cleanly and keep all tests passing on both targets. Protocol changes should include focused regression tests where practical.

## Cutting a release

Releases are cut with `make release`, which requires a clean working tree and only plain `git` — no GitHub-specific tooling. It builds `linux` and `windows-cross` (both format, build, and run their full test suite first — nothing untested gets released), then:

1. shows the current version (read from the `VERSION` file at the repo root) and prompts for the new one — it never bumps the version automatically, and rejects anything that isn't a valid `MAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH-prerelease` semantic version (or is identical to the current one; a version that doesn't look like an increase gets a warning, not a hard block);
2. packages `ts-cli-v<version>-linux-x86_64` (the Linux binary) and `ts-cli-v<version>-windows-x86_64.zip` and `ts-gtk-v<version>-windows-x86_64.zip` (the `build/windows-cross/dist/` folders) into `build/release/`;
3. asks for a final confirmation, showing exactly what it's about to do;
4. on confirmation: commits the `VERSION` bump, creates an annotated `v<version>` tag, and pushes both to `origin`.

`make release` stops there — it does not create anything on GitHub itself. A maintainer creates the actual GitHub release from the pushed tag by hand and attaches the three files left behind in `build/release/`. Answering anything other than `y` at the confirmation prompt aborts before anything is committed, tagged, or pushed.

## Project boundaries

Keep responsibilities separated:

- `config/` — configuration: XDG/AppData paths, `config.conf`, the identity file, per-user settings, first-run setup, and applying saved audio settings to an engine; shared by every front end
- `client/` — CLI, application runtime and UI/audio coordination
- `client-gtk/` — GTK front end; presentation only, on top of `config/` and `client/`'s runtime and command handling
- `log/` — logging and sinks
- `net/` — networking primitives
- `protocol/` — TeamSpeak wire protocol, crypto, transport, typed messages and protocol state

The protocol layer must not read configuration files, inspect XDG paths, or depend on terminal/UI behavior. Front ends must not parse or write configuration files themselves; anything a second front end would duplicate belongs in `config/`.

Prefer typed messages and state objects over raw command strings in higher layers. Avoid generic `Utils` classes and large procedural grab-bags.

## Platform backends

`ts-cli` targets Linux natively and Windows via MinGW-w64 cross-compilation (see [BUILDING.md](BUILDING.md)), so public headers must not leak OS-specific types (`sockaddr_storage`, `pollfd`, file descriptors, `HANDLE`, ...). `net`, `log`, and `audio` follow the same shape for this:

- a public, platform-agnostic interface (`net::UdpSocket`, `log::TerminalColor`, `audio::AudioEngine`) that the rest of the codebase links against;
- an abstract backend interface (`net::SocketBackend`, `audio::AudioBackend`) or a set of free functions, plus a `Create*Backend()` factory declared next to it where a stateful backend is needed;
- one concrete backend implementation per build target, kept in its own source subdirectory (`net/src/posix/` + `net/src/win32/`, `log/src/posix/` + `log/src/win32/`, `audio/src/posix/pipewire_backend.cpp` + `audio/src/win32/wasapi_backend.cpp`) and selected by `CMakeLists.txt` based on the target platform — never by `#ifdef` scattered through shared source.

`config/platform/paths.hpp` and `config/platform/secure_file.hpp` (`config/src/posix/` + `config/src/win32/`) follow the same pattern for the per-user config directory and owner-only file permissions. `client/platform/console_io.hpp`, `client/platform/terminal_control.hpp` and `client/gtk/platform/shutdown_signal.hpp` do too, for interactive-terminal/stdin handling and cursor control (`client/src/platform/posix/`, `client/src/platform/win32/`, `client-gtk/src/platform/posix/`, `client-gtk/src/platform/win32/`), since those are also inherently OS-specific. `audio` ships a real backend on both targets now (PipeWire/Opus on Linux, WASAPI/Opus on Windows); Opus and RNNoise (`audio/src/opus_codec.cpp`, `audio/src/rnnoise_filter.cpp`) are platform-agnostic and stay outside the `posix/`/`win32/` split entirely, wired in once regardless of which backend gets picked.

Link libraries dynamically on every target. On Linux that means the distribution's shared libraries, so security fixes (OpenSSL above all) arrive through the package manager. On Windows, where nothing provides those libraries, each executable is shipped as a folder with the DLLs it imports, taken from MSYS2's UCRT64 packages (see [BUILDING.md](BUILDING.md#output)). Don't add `-static` link flags: GCC applies them to the whole link, and a static C++ runtime in an executable next to DLLs that use the shared one is a bug waiting to happen. A new Windows executable gets its folder from `ts_bundle_windows()` (`cmake/bundle_windows_target.cmake`).

When adding a backend for a target that doesn't have one yet, this is the checklist: implement the missing backend against the existing interface headers, add the corresponding `CMakeLists.txt` branch, and leave every consumer above the backend layer untouched. If a new primitive needs OS-specific types at all, give it the same treatment rather than including platform headers from a public header.

## C++ style

- C++20
- Traditional include guards; do not use `#pragma once`
- Lowercase snake_case filenames
- PascalCase types and functions
- Private fields use `m_` followed by PascalCase, for example `m_ClientId`
- Do not write `this->m_Field` unless required
- Opening braces stay on the same line
- Prefer `std::endl` for normal terminal/log output in this project

Namespaces follow the component structure, for example:

```cpp
namespace ts::net {
}

namespace ts::protocol {
}
```

## Includes

In header files:

- Use quotes only for project headers in the same directory
- Use angle brackets for project headers from other directories
- Use angle brackets for standard-library and external headers

Example:

```cpp
#include "bootstrap.hpp"

#include <protocol/crypto/session_crypto.hpp>
#include <cstdint>
#include <string>
```

In `.cpp` files, use angle brackets for project headers:

```cpp
#include <protocol/session/session.hpp>
```

## `[[nodiscard]]`

Use `[[nodiscard]]` for values whose result should normally be consumed:

- parsers
- factories
- serializers
- getters
- computations
- data-returning functions

Do not add it to actions or lifecycle operations such as:

```cpp
Send();
Connect();
Run();
```

An action does not become `[[nodiscard]]` merely because it returns an ID.

## Protocol work

Prefer evidence in this order:

1. behavior observed from real TeamSpeak clients/servers
2. established protocol implementations and documentation
3. inference only when necessary

Do not weaken crypto or transport validation just to make a higher-level feature work. If a protocol assumption fails against a real server, capture the actual message/packet behavior and fix the correct layer.

Keep protocol changes narrow. Avoid combining unrelated refactors with new wire behavior.

## Session and transport architecture

One network I/O thread should eventually own all TeamSpeak protocol state, including:

- packet sequences and generations
- ACK/retransmission state
- crypto
- fragmentation and compression
- ping/pong and timers
- session state mutations

Do not process packets concurrently on separate worker threads. The UI should communicate with the network side through queued actions/events. Audio will live on its own thread later.

## State handling

Treat the server as the source of truth. For actions such as moving channels, send the command and let the resulting server notification update local state rather than mutating state optimistically.

Preserve unknown or not-yet-supported notifications when possible instead of silently dropping information required by later features.

## Scope

The goal is a useful terminal TeamSpeak client, not complete implementation of every TeamSpeak feature. Current priorities are navigation, text chat, interactive runtime behavior and voice/audio.

Administrative tooling, exhaustive permission management and unrelated protocol surface should only be added when there is a concrete client need.

## License

Contributions are made under the same license as the project: `GPL-3.0-or-later`.
