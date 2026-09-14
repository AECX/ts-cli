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
| **Windows** (x86_64) | cross-compiled from Linux via MinGW-w64, tested under Wine | Supported — networking, protocol, CLI, and WASAPI/Opus voice are all ported and verified on real Windows hardware; requires the `mingw-w64-opus` AUR package to build with voice enabled (see [Current limitations](#current-limitations)) |
| **Windows** (x86_64) | built natively on Windows via [MSYS2](https://www.msys2.org/)'s UCRT64 environment | Same code, same status as the row above — see [Windows (native, via MSYS2)](#windows-native-via-msys2) |

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
make linux
```

This formats the source tree with `clang-format`, configures and builds the project, and runs the full test suite. The resulting binary is:

```text
build/linux/client/ts-cli
```

If you'd rather drive the steps yourself (useful for iterative development):

```bash
cmake -S . -B build/linux
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
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
make windows-cross
```

Or drive it manually:

```bash
cmake -S . -B build/windows-cross -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
cmake --build build/windows-cross
ctest --test-dir build/windows-cross --output-on-failure
```

The resulting binary is:

```text
build/windows-cross/client/ts-cli.exe
```

It is statically linked, so it runs as-is under Wine or on a bare Windows machine — no MinGW runtime DLLs required.

### Running it under Wine

```bash
wine build/windows-cross/client/ts-cli.exe voice.example.net:9987
```

Console coloring and cursor control on Windows use the native Console API rather than ANSI escape sequences, so this renders correctly both under Wine and on real Windows consoles.

### Current limitations

Audio on Windows uses a real WASAPI backend (shared-mode, event-driven capture and render, device enumeration and switching) — see `audio/src/win32/wasapi_backend.cpp`. A few things to know:

* it requires `mingw-w64-opus` to build at all (see [Requirements](#requirements-1) above); without it, build with `-DTS_AUDIO_STUB_DEPS=ON` to get everything except voice;
* exclusive mode and live device hot-plug notifications aren't implemented — device changes while the client is running need `/audio input`/`/audio output` to pick up the new device, rather than switching automatically;
* this backend has been exercised on real Windows hardware — capture and playback behave as they do on PipeWire — as well as under Wine (device enumeration, capture/render start/stop, device switching, and error rejection for an invalid device). If you do hit a glitch or a device that won't enumerate correctly, `audio/src/win32/wasapi_backend.cpp` is the place to look, and `audio/include/audio/audio_backend.hpp` is the interface it implements.

### Toolchain internals

The toolchain file at [`cmake/toolchains/mingw-w64.cmake`](cmake/toolchains/mingw-w64.cmake) is heavily commented and documents the non-obvious parts directly:

* why static linking is used (`-static -static-libgcc -static-libstdc++`) and why `find_library()` must be told to prefer `.a` over `.dll.a`, or the binary silently ends up dynamically linked against DLLs instead;
* why that library-suffix preference is actually applied in the root `CMakeLists.txt` rather than the toolchain file (CMake's Windows-GNU platform module resets it during `project()`);
* the AUR `mingw-w64-openssl-3` package's non-standard header/library layout and how `OPENSSL_INCLUDE_DIR`/`LIB_EAY`/`SSL_EAY` are pre-seeded to work around it.

Read that file if you're debugging a Windows build issue or porting the toolchain to another distribution.

---

## Windows (native, via MSYS2)

`ts-cli` doesn't need to be cross-compiled — it can be built directly on Windows using [MSYS2](https://www.msys2.org/)'s UCRT64 environment, the same GCC/MinGW-w64 toolchain family the Linux cross-compile above targets, just running natively on Windows instead of under Wine.

> [!TIP]
> **Use MSYS2's UCRT64 environment, not Cygwin.** Cygwin links against `cygwin1.dll`, a POSIX-compatibility runtime that isn't part of a bare Windows install — the opposite of what this build produces, a native Win32 binary that talks to the Windows API directly. MSYS2's UCRT64 environment gives you that plus a real package manager (`pacman`) for the dependencies below — a bare/manual MinGW install has neither advantage. If you have a specific reason to need Cygwin regardless, the general shape of these instructions still applies, but expect to hunt down `.pc` files and static-vs-shared linking yourself.

### Requirements

1. Install MSYS2 from [msys2.org](https://www.msys2.org/), then open the **"MSYS2 UCRT64"** shell from the Start menu — not the plain "MSYS2" shell (POSIX-emulated environment) and not "MSYS2 MinGW x64" (the older, legacy MSVCRT-based environment).
2. Update packages and install the toolchain and dependencies:

   ```bash
   pacman -Syu
   pacman -S \
     mingw-w64-ucrt-x86_64-gcc \
     mingw-w64-ucrt-x86_64-cmake \
     mingw-w64-ucrt-x86_64-ninja \
     mingw-w64-ucrt-x86_64-pkgconf \
     mingw-w64-ucrt-x86_64-clang-tools-extra \
     mingw-w64-ucrt-x86_64-openssl \
     mingw-w64-ucrt-x86_64-libsodium \
     mingw-w64-ucrt-x86_64-opus \
     make
   ```

   `mingw-w64-ucrt-x86_64-clang-tools-extra` provides `clang-format` (used by `make format`, which every Makefile target runs first — see *Build* below). The plain `make` package (not `mingw-w64-ucrt-x86_64-make`, which installs as `mingw32-make` instead) gives you the literal `make` command needed to drive the Makefile. RNNoise is optional, same as on Linux: add `mingw-w64-ucrt-x86_64-rnnoise` if it's available in your MSYS2 repos, or skip it and the build degrades gracefully (`/audio filter rnnoise` just stays unavailable).

   If `pacman -Syu` asks you to close and reopen the shell partway through, do so and rerun it — that's normal MSYS2 behavior (a core-package self-update), not specific to this project.

### Build

From the same "MSYS2 UCRT64" shell, in the repository root:

```bash
make windows-msys
```

`make windows-msys` refuses to run outside an MSYS2 UCRT64 shell (it checks `$MSYSTEM`/`$MINGW_PREFIX`) and verifies `cmake`, `ninja`, `pkg-config`, `gcc`, `g++`, `cygpath`, and the `libsodium`/`opus`/`openssl` pkg-config modules are all found before configuring, so a missing dependency fails fast with a clear message instead of a confusing error partway through CMake.

Or drive it manually:

```bash
env -u CMAKE_TOOLCHAIN_FILE -u PKG_CONFIG_SYSROOT_DIR -u PKG_CONFIG_LIBDIR -u PKG_CONFIG_PATH \
  cmake -S . -B build/windows-msys -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR="$(cygpath -m "$MINGW_PREFIX")" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE
cmake --build build/windows-msys
ctest --test-dir build/windows-msys --output-on-failure
```

The `env -u ...` prefix keeps a stray `CMAKE_TOOLCHAIN_FILE`/`PKG_CONFIG_*` setting left over from a Linux cross-build shell from leaking into this native build. `-DOPENSSL_ROOT_DIR`/`-DOPENSSL_USE_STATIC_LIBS` point CMake at UCRT64's static OpenSSL explicitly, since this build has no toolchain file of its own to pre-seed those paths the way [`cmake/toolchains/mingw-w64.cmake`](cmake/toolchains/mingw-w64.cmake) does for the cross-compiled build (see *Toolchain internals* above).

The resulting binary is:

```text
build/windows-msys/client/ts-cli.exe
```

identical in behavior to `build/windows-cross/client/ts-cli.exe` from the cross-compiled build — `WIN32`/`MINGW` are set by CMake based on the compiler's target, not by which machine is doing the compiling, so it's the same WASAPI backend, the same networking/protocol/CLI code, no path-specific branching anywhere. Note one real difference, though: unlike the cross-compiled build, this configuration does not pass `-static`/`-static-libgcc`/`-static-libstdc++`, so beyond OpenSSL (statically linked via `-DOPENSSL_USE_STATIC_LIBS=TRUE` above) the resulting `.exe` runs fine from within the MSYS2 UCRT64 shell (`ucrt64/bin` is already on `PATH` there) but may depend on that environment's runtime DLLs if you copy it out to a bare Windows machine. Add the same `-DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"` the cross-compiled build uses to the manual command above if you need a fully portable binary.

> [!NOTE]
> This path hasn't been exercised by the person who wrote it — the reference environment for this project cross-compiles from Linux under Wine, not a native Windows/MSYS2 box. If you hit an MSYS2-specific packaging quirk (a misplaced `.pc` file, a static-vs-import-library mismatch), it'll likely resemble the AUR `openssl-3` quirk documented under *Toolchain internals* above, and the same kind of fix (pointing CMake straight at the real paths) should apply. Please report back what you find.

---

## Troubleshooting

* **`ts-client-tests.exe` link errors mentioning `libsodium` or `libcrypto`** — the AUR packages weren't found, or `CMAKE_FIND_LIBRARY_SUFFIXES` picked a `.dll.a` import library instead of the static archive. Re-check the requirements above and see *Toolchain internals*.
* **`windns.h` / `VOID` / `ULONG` compile errors on Windows sources** — `windows.h` must be included before `windns.h`. This is already handled in-tree with `clang-format off` guards around the affected includes; if you hit this in new code, do the same.
* **`wine: command not found` while running `make windows-cross`** — Wine wasn't found, so cross-compiled tests can't run automatically; the configure step will warn but still produce `build/windows-cross/client/ts-cli.exe`, which you can copy to a real Windows machine instead.

---

## Next steps

Once you can build it, see [CONTRIBUTING.md](CONTRIBUTING.md) for code style, project boundaries, and the platform-backend pattern to follow when extending Linux or Windows support.
