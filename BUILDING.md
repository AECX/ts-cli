# Building ts-cli

This document covers everything needed to set up a build environment for `ts-cli` and produce a working binary — for Linux, and for Windows either cross-compiled from Linux or built natively.

For usage, commands, and configuration, see [README.md](README.md). For code style and contribution conventions, see [CONTRIBUTING.md](CONTRIBUTING.md).

<p align="center">
  <img src="assets/ping/happy.png" alt="ts-cli mascot excited about a terminal" width="260">
</p>

---

## Supported targets

| Target | Method | Status |
| --- | --- | --- |
| **Linux** (native) | direct build via CMake | Fully supported — networking, protocol, CLI, and PipeWire/Opus voice |
| **Windows** (x86_64) | cross-compiled from Linux via MinGW-w64, tested under Wine | Experimental — networking, protocol, CLI, and WASAPI/Opus voice are all ported; requires the `mingw-w64-opus` AUR package to build with voice enabled (see [Current limitations](#current-limitations)) |
| **Windows** (x86_64) | built natively on Windows via [MSYS2](https://www.msys2.org/)'s MinGW-w64 environment | Same code, same status as the row above — see [Windows (native, via MSYS2)](#windows-native-via-msys2) |

Both targets share the same source tree. `net`, `log`, `audio`, and `client/platform` each expose a platform-agnostic public interface with a POSIX and a Win32 backend selected in `CMakeLists.txt` — see [Platform backends](CONTRIBUTING.md#platform-backends) in `CONTRIBUTING.md` if you're implementing or extending one.

---

## Linux

### Requirements

* C++20 compiler (GCC or Clang)
* CMake 3.20+
* OpenSSL
* libsodium
* clang-format
* PipeWire 0.3
* Opus

Optional:

* RNNoise — enables `/audio filter rnnoise`. Must expose the `rnnoise` pkg-config module for CMake to detect it.

#### Arch Linux

```bash
sudo pacman -Syu --needed base-devel cmake clang pkgconf git openssl libsodium pipewire opus rnnoise
```

#### Debian / Ubuntu

```bash
sudo apt update && sudo apt install -y build-essential cmake clang clang-format pkg-config git make libssl-dev libsodium-dev libpipewire-0.3-dev libopus-dev librnnoise-dev
```

### Build

```bash
./build.sh
```

This formats the source tree with `clang-format`, configures and builds the project, and runs the full test suite. The resulting binary is:

```text
build/client/ts-cli
```

If you'd rather drive the steps yourself (useful for iterative development):

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Windows (cross-compiled via MinGW-w64)

`ts-cli` does not need to be built on Windows itself. The Windows target is produced by cross-compiling from Linux with MinGW-w64, links statically (no runtime DLL dependencies), and its test suite runs transparently through Wine via CMake's `CMAKE_CROSSCOMPILING_EMULATOR`.

> [!NOTE]
> This toolchain is set up and verified on **Arch Linux**. Other distributions will need an equivalent `x86_64-w64-mingw32` GCC toolchain plus statically-built libsodium and OpenSSL for that target — if your distribution doesn't package those, you'll need to build them from source yourself.

### Requirements

From the official Arch repositories:

```bash
sudo pacman -S mingw-w64-gcc mingw-w64-binutils mingw-w64-crt mingw-w64-headers mingw-w64-winpthreads
```

From the AUR (via your preferred helper, e.g. `yay` or `paru`):

```bash
yay -S mingw-w64-libsodium mingw-w64-openssl-3 mingw-w64-opus
```

`mingw-w64-opus` is required for voice; without it, `pkg_check_modules(OPUS REQUIRED ...)` fails at configure time with a clear error. If you just want to build and test everything except voice, configure with `-DTS_AUDIO_STUB_DEPS=ON` instead of installing it (see [Current limitations](#current-limitations)). RNNoise capture filtering is optional on Windows exactly as it is on Linux — install a `mingw-w64-rnnoise`-equivalent package if one is available for your setup; the build degrades gracefully (`/audio filter rnnoise` just stays unavailable) if it isn't.

And Wine, to run and test the cross-compiled binary:

```bash
sudo pacman -S wine
```

> [!TIP]
> Wine lives in the `multilib` repository on Arch — enable it in `/etc/pacman.conf` first if `wine` isn't found.

### Build

```bash
./build-windows.sh
```

Or drive it manually:

```bash
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
cmake --build build-windows
ctest --test-dir build-windows --output-on-failure
```

The resulting binary is:

```text
build-windows/client/ts-cli.exe
```

It is statically linked, so it runs as-is under Wine or on a bare Windows machine — no MinGW runtime DLLs required.

### Running it under Wine

```bash
wine build-windows/client/ts-cli.exe voice.example.net:9987
```

Console coloring and cursor control on Windows use the native Console API rather than ANSI escape sequences, so this renders correctly both under Wine and on real Windows consoles.

### Current limitations

Audio on Windows uses a real WASAPI backend (shared-mode, event-driven capture and render, device enumeration and switching) — see `audio/src/win32/wasapi_backend.cpp`. A few things to know:

* it requires `mingw-w64-opus` to build at all (see [Requirements](#requirements) above); without it, build with `-DTS_AUDIO_STUB_DEPS=ON` to get everything except voice;
* exclusive mode and live device hot-plug notifications aren't implemented — device changes while the client is running need `/audio input`/`/audio output` to pick up the new device, rather than switching automatically;
* this backend hasn't been exercised on real Windows hardware by the person who wrote it — it's been validated under Wine (device enumeration, capture/render start/stop, device switching, and error rejection for an invalid device all work there against Wine's own audio backend), but Wine's audio path isn't a substitute for a real WASAPI driver stack. If you hit a glitch or a device that won't enumerate correctly on real hardware, `audio/src/win32/wasapi_backend.cpp` is the place to look, and `audio/include/audio/audio_backend.hpp` is the interface it implements.

### Toolchain internals

The toolchain file at [`cmake/toolchains/mingw-w64.cmake`](cmake/toolchains/mingw-w64.cmake) is heavily commented and documents the non-obvious parts directly:

* why static linking is used (`-static -static-libgcc -static-libstdc++`) and why `find_library()` must be told to prefer `.a` over `.dll.a`, or the binary silently ends up dynamically linked against DLLs instead;
* why that library-suffix preference is actually applied in the root `CMakeLists.txt` rather than the toolchain file (CMake's Windows-GNU platform module resets it during `project()`);
* the AUR `mingw-w64-openssl-3` package's non-standard header/library layout and how `OPENSSL_INCLUDE_DIR`/`LIB_EAY`/`SSL_EAY` are pre-seeded to work around it.

Read that file if you're debugging a Windows build issue or porting the toolchain to another distribution.

---

## Windows (native, via MSYS2)

`ts-cli` doesn't need to be cross-compiled — it can be built directly on Windows using [MSYS2](https://www.msys2.org/)'s MinGW-w64 (`mingw64`) environment, the same GCC/MinGW-w64 toolchain family the Linux cross-compile above targets, just running natively on Windows instead of under Wine.

> [!TIP]
> **Use MSYS2's `mingw64` environment, not Cygwin.** Cygwin links against `cygwin1.dll`, a POSIX-compatibility runtime that isn't part of a bare Windows install — the opposite of what the MinGW-w64 target is for here, which is a statically-linked `.exe` with no runtime dependency beyond the OS itself (see *It is statically linked* above). MSYS2's `mingw64` environment produces genuinely native Win32 binaries with no such shim, using the identical MinGW-w64 toolchain the cross-compiled build already targets, and gives you a real package manager (`pacman`) for the dependencies below — a bare/manual MinGW install has neither advantage. If you have a specific reason to need Cygwin regardless, the general shape of these instructions still applies, but expect to hunt down `.pc` files and static-vs-shared linking yourself.

### Requirements

1. Install MSYS2 from [msys2.org](https://www.msys2.org/), then open the **"MSYS2 MinGW x64"** shell from the Start menu — not the plain "MSYS2" shell, which targets the POSIX-emulated environment rather than native Win32.
2. Update packages and install the toolchain and dependencies:

   ```bash
   pacman -Syu
   pacman -S \
     mingw-w64-x86_64-gcc \
     mingw-w64-x86_64-cmake \
     mingw-w64-x86_64-ninja \
     mingw-w64-x86_64-pkgconf \
     mingw-w64-x86_64-clang-tools-extra \
     mingw-w64-x86_64-openssl \
     mingw-w64-x86_64-libsodium \
     mingw-w64-x86_64-opus
   ```

   `mingw-w64-x86_64-clang-tools-extra` provides `clang-format` (used by the formatting step `./build.sh` normally runs — see *Build* below). RNNoise is optional, same as on Linux: add `mingw-w64-x86_64-rnnoise` if it's available in your MSYS2 repos, or skip it and the build degrades gracefully (`/audio filter rnnoise` just stays unavailable).

   If `pacman -Syu` asks you to close and reopen the shell partway through, do so and rerun it — that's normal MSYS2 behavior (a core-package self-update), not specific to this project.

### Build

From the same "MSYS2 MinGW x64" shell, in the repository root:

```bash
clang-format -i $(find . -name '*.cpp' -o -name '*.hpp')
cmake -S . -B build -G Ninja -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build
ctest --test-dir build --output-on-failure
```

The `-DCMAKE_EXE_LINKER_FLAGS` above is exactly what [`cmake/toolchains/mingw-w64.cmake`](cmake/toolchains/mingw-w64.cmake) sets automatically for the cross-compiled build (see *Toolchain internals* above) — it statically links the MinGW runtime/thread library so the resulting `.exe` doesn't need `libstdc++-6.dll`/`libgcc_s_seh-1.dll`/`libwinpthread-1.dll` on `PATH`. It isn't required for the build to succeed — without it the binary still runs fine from within the MSYS2 shell (those DLLs live in `mingw64/bin`, already on `PATH` there) — but it matters if you want to copy the `.exe` out to a machine that doesn't have MSYS2 installed. `./build.sh`/`./build-windows.sh` are written for a Linux dev shell and won't run as-is here, hence driving the steps manually above instead.

The resulting binary is:

```text
build/client/ts-cli.exe
```

identical in behavior to `build-windows/client/ts-cli.exe` from the cross-compiled build — `WIN32`/`MINGW` are set by CMake based on the compiler's target, not by which machine is doing the compiling, so it's the same WASAPI backend, the same networking/protocol/CLI code, no path-specific branching anywhere.

> [!NOTE]
> This path hasn't been exercised by the person who wrote it — the reference environment for this project cross-compiles from Linux under Wine, not a native Windows/MSYS2 box. If you hit an MSYS2-specific packaging quirk (a misplaced `.pc` file, a static-vs-import-library mismatch), it'll likely resemble the AUR `openssl-3` quirk documented under *Toolchain internals* above, and the same kind of fix (pointing CMake straight at the real paths) should apply. Please report back what you find.

---

## Troubleshooting

* **`ts-client-tests.exe` link errors mentioning `libsodium` or `libcrypto`** — the AUR packages weren't found, or `CMAKE_FIND_LIBRARY_SUFFIXES` picked a `.dll.a` import library instead of the static archive. Re-check the requirements above and see *Toolchain internals*.
* **`windns.h` / `VOID` / `ULONG` compile errors on Windows sources** — `windows.h` must be included before `windns.h`. This is already handled in-tree with `clang-format off` guards around the affected includes; if you hit this in new code, do the same.
* **`wine: command not found` while running `./build-windows.sh`** — Wine wasn't found, so cross-compiled tests can't run automatically; the configure step will warn but still produce `build-windows/client/ts-cli.exe`, which you can copy to a real Windows machine instead.

---

## Next steps

Once you can build it, see [CONTRIBUTING.md](CONTRIBUTING.md) for code style, project boundaries, and the platform-backend pattern to follow when extending Linux or Windows support.
