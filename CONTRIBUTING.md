# Contributing to ts-cli

`ts-cli` is an experimental TeamSpeak client in C++20 for Linux and Windows. Build setup for every target is in [BUILDING.md](BUILDING.md). Contributions are made under the project's license, `GPL-3.0-or-later`.

## Workflow

### Issues

Every change starts with an issue. Each issue gets exactly one main label:

| Label           | Use for                                                            |
|-----------------|--------------------------------------------------------------------|
| `feature`       | something new                                                      |
| `bug`           | fixing wrong behavior without changing the intended behavior       |
| `change`        | changing intended behavior                                         |
| `update`        | changes that don't alter behavior (visuals, refactoring, deps)     |
| `documentation` | anything documentation                                             |
| `hotfix`        | an urgent fix that goes straight to `master`                       |

Optional sub-labels narrow it down and can be combined:

- **Scope**, named after the component: `scope:protocol`, `scope:net`, `scope:audio`, `scope:log`, `scope:config`, `scope:client` (CLI and runtime), `scope:gtk` (`client-gtk/`), `scope:build` (CMake, Makefile, packaging, releases)
- **Platform**, only if the issue is OS-specific: `platform:linux`, `platform:windows`
- **Status**: `status:needs_discussion` (the approach isn't agreed on yet), `status:needs_investigation` (the cause or the expected behavior still has to be worked out)
- **Priority**: `priority:0` to `priority:5`, where `0` is the most urgent and `5` can wait indefinitely

### Branches

- `master` reflects the latest release; every release is a `vX.Y.Z[-prerelease]` tag on it.
- `dev` is the integration branch.
- Work happens on `<main-label>/<issue>-<short-slug>`, branched from `dev`, e.g. `feature/12-inspect-user-client` or `change/14-dont-mute-by-default`. Hotfix branches start from `master`.

### Pull requests

- Open the PR against `dev`; `hotfix/*` PRs go against `master`.
- One issue per PR. Commit messages follow `[<scope>] <Imperative summary> (#<issue>)`:
  - the scope is the scope label without its prefix (`protocol`, `audio`, `gtk`, `build`, ...), or `docs` for documentation-only commits
  - ideally one scope per commit; if a change really spans two, list both with the main one first (`[config][gtk]`), otherwise use the scope the change originates from
  - the summary is capitalized, has no trailing period, and the whole line stays around 72 characters

  ```
  [audio] Pace audio playback by the output device clock (#3)
  [config,gtk] Manage the identity in the GTK options (#3)
  ```

- Rebase onto the target branch before merging. PRs are merged with rebase-merge, keeping the individual commits and a linear history.
- `make linux` must pass. If you touched `net`, `protocol`, `log`, `audio` or `config`, `make windows-cross` must pass as well.
- Protocol changes come with focused regression tests where practical.

### Releases

There is no fixed schedule; maintainers release when it makes sense.

1. Fast-forward `master` to `dev`.
2. Run `make release` on `master`. It builds and tests both targets, prompts for the new version, commits `VERSION` as `[release] v<version>`, tags `v<version>` and pushes both.
3. Create the GitHub release from the tag and attach the three files in `build/release/`.
4. Fast-forward `dev` to `master`.

A hotfix is released the same way straight from `master` (usually a patch version); afterwards merge `master` back into `dev`.

## Architecture

- `config/` — paths, `config.conf`, identity, per-user settings, first-run setup; shared by every front end
- `client/` — CLI, application runtime, UI/audio coordination
- `client-gtk/` — GTK front end; presentation only, on top of `config/` and `ts-client-runtime`
- `log/` — logging and sinks
- `net/` — networking primitives
- `protocol/` — TeamSpeak wire protocol, crypto, transport, typed messages, protocol state

Rules:

- The protocol layer never reads configuration, inspects paths or depends on terminal/UI behavior.
- Front ends never parse or write configuration files; anything a second front end would duplicate belongs in `config/`.
- Prefer typed messages and state objects over raw command strings in higher layers. No generic `Utils` grab-bags.
- One network thread owns all protocol state (sequences, ACKs, crypto, fragmentation, timers). Never process packets concurrently; the UI talks to it through queued actions and events.
- The server is the source of truth: send the command and let the resulting notification update local state, no optimistic updates. Preserve unknown notifications instead of dropping them.

### Platform backends

- Public headers never leak OS-specific types (`sockaddr_storage`, `pollfd`, file descriptors, `HANDLE`, ...).
- A platform-agnostic interface (e.g. `net::UdpSocket`, `audio::AudioEngine`) sits on top of an abstract backend or free functions, with a `Create*Backend()` factory where state is needed.
- Each target gets its own implementation in `src/posix/` or `src/win32/`, selected in `CMakeLists.txt`, never by `#ifdef` in shared code. Platform-agnostic parts (Opus, RNNoise) stay outside the split.
- Link dynamically on every target and never add `-static`. A new Windows executable gets its DLL folder from `ts_bundle_windows()` (`cmake/bundle_windows_target.cmake`).

### Protocol work

- Prefer evidence from real TeamSpeak clients/servers, then established implementations and documentation, and only then inference.
- Never weaken crypto or transport validation to make a higher-level feature work; capture the real behavior and fix the correct layer.
- Keep protocol changes narrow; don't mix unrelated refactors with new wire behavior.

## Code style

- C++20, traditional include guards (no `#pragma once`), opening braces on the same line
- snake_case filenames, PascalCase types and functions, private fields `m_PascalCase` (e.g. `m_ClientId`), no `this->m_Field` unless required
- Namespaces follow the components: `ts::net`, `ts::protocol`, ...
- Prefer `std::endl` for terminal/log output
- Includes: quotes only for project headers in the same directory (headers only), angle brackets for everything else

  ```cpp
  #include "bootstrap.hpp"

  #include <protocol/crypto/session_crypto.hpp>
  #include <string>
  ```

- `[[nodiscard]]` on parsers, factories, serializers, getters and other data-returning functions; not on actions such as `Send()`, `Connect()` or `Run()`, even if they return an ID

## Scope

The goal is a useful TeamSpeak client, not every TeamSpeak feature. Current priorities are navigation, text chat, interactive runtime behavior and voice. Administrative tooling and exhaustive permission management only get added for a concrete client need.
