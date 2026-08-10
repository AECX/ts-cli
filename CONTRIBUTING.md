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
2. packages the two binaries as `ts-cli-v<version>-linux-x86_64` and `ts-cli-v<version>-windows-x86_64.exe` into `build/release/`;
3. asks for a final confirmation, showing exactly what it's about to do;
4. on confirmation: commits the `VERSION` bump, creates an annotated `v<version>` tag, and pushes both to `origin`.

`make release` stops there — it does not create anything on GitHub itself. A maintainer creates the actual GitHub release from the pushed tag by hand and attaches the two binaries left behind in `build/release/`. Answering anything other than `y` at the confirmation prompt aborts before anything is committed, tagged, or pushed.

## Project boundaries

Keep responsibilities separated:

- `client/` — CLI, configuration, application runtime and later UI/audio coordination
- `log/` — logging and sinks
- `net/` — networking primitives
- `protocol/` — TeamSpeak wire protocol, crypto, transport, typed messages and protocol state

The protocol layer must not read configuration files, inspect XDG paths, or depend on terminal/UI behavior.

Prefer typed messages and state objects over raw command strings in higher layers. Avoid generic `Utils` classes and large procedural grab-bags.

## Platform backends

`ts-cli` targets Linux natively and Windows via MinGW-w64 cross-compilation (see [BUILDING.md](BUILDING.md)), so public headers must not leak OS-specific types (`sockaddr_storage`, `pollfd`, file descriptors, `HANDLE`, ...). `net`, `log`, and `audio` follow the same shape for this:

- a public, platform-agnostic interface (`net::UdpSocket`, `log::TerminalColor`, `audio::AudioEngine`) that the rest of the codebase links against;
- an abstract backend interface (`net::SocketBackend`, `audio::AudioBackend`) or a set of free functions, plus a `Create*Backend()` factory declared next to it where a stateful backend is needed;
- one concrete backend implementation per build target, kept in its own source subdirectory (`net/src/posix/` + `net/src/win32/`, `log/src/posix/` + `log/src/win32/`, `audio/src/posix/pipewire_backend.cpp` + `audio/src/win32/wasapi_backend.cpp`) and selected by `CMakeLists.txt` based on the target platform — never by `#ifdef` scattered through shared source.

`client/platform/console_io.hpp` and `client/platform/terminal_control.hpp` follow the same pattern for interactive-terminal/stdin handling and cursor control (`client/src/platform/posix/`, `client/src/platform/win32/`), since those are also inherently OS-specific. `audio` ships a real backend on both targets now (PipeWire/Opus on Linux, WASAPI/Opus on Windows); Opus and RNNoise (`audio/src/opus_codec.cpp`, `audio/src/rnnoise_filter.cpp`) are platform-agnostic and stay outside the `posix/`/`win32/` split entirely, wired in once regardless of which backend gets picked.

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
