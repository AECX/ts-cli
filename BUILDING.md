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
| **Windows** (x86_64) | cross-compiled from Linux via MinGW-w64, tested under Wine | Supported — networking, protocol, CLI, and WASAPI/Opus voice are all ported and verified on real Windows hardware; Windows libraries come prebuilt from MSYS2 and the build produces a self-contained folder |
| **Windows** (x86_64) | built natively on Windows via [MSYS2](https://www.msys2.org/)'s UCRT64 environment | Same code, same status as the row above — see [Windows (native, via MSYS2)](#windows-native-via-msys2) |

Both targets share the same source tree. `net`, `log`, `audio`, `config/platform` and `client/platform` each expose a platform-agnostic public interface with a POSIX and a Win32 backend selected in `CMakeLists.txt` — see [Platform backends](CONTRIBUTING.md#platform-backends) in `CONTRIBUTING.md` if you're implementing or extending one.

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

`ts-cli` does not need to be built on Windows itself. The Windows target is produced by cross-compiling from Linux with MinGW-w64, and its test suite runs transparently through Wine via CMake's `CMAKE_CROSSCOMPILING_EMULATOR`.

Like the Linux build, it links its libraries dynamically. Windows has no package manager that would provide OpenSSL, libsodium or Opus, so each program is shipped the usual Windows way: as a folder with the DLLs it uses next to it (see [Output](#output)).

> [!NOTE]
> This toolchain is set up and verified on **Arch Linux**. Other distributions need an equivalent `x86_64-w64-mingw32` GCC toolchain targeting the UCRT runtime; the libraries come from MSYS2 either way.

### Requirements

From the official Arch repositories:

```bash
sudo pacman -S mingw-w64-gcc mingw-w64-binutils mingw-w64-crt mingw-w64-headers mingw-w64-winpthreads
```

And Wine, to run and test the cross-compiled binaries:

```bash
sudo pacman -S wine
```

> [!TIP]
> Wine lives in the `multilib` repository on Arch — enable it in `/etc/pacman.conf` first if `wine` isn't found.

Plus Python 3.14 or newer (for its built-in zstd support) and network access on the first build. The libraries themselves (OpenSSL, libsodium, Opus and RNNoise) aren't cross-built or taken from the AUR. [`cmake/msys2/fetch_packages.py`](cmake/msys2/fetch_packages.py) downloads MSYS2's prebuilt UCRT64 packages, built with the same GCC and C runtime as `mingw-w64-gcc`, together with their runtime dependencies. It verifies each against the SHA-256 in MSYS2's repository database and unpacks them into `build/deps/msys2-ucrt64/`. Later builds reuse that copy.

### Build

```bash
make windows-cross
```

Or drive it manually:

```bash
python3 -I cmake/msys2/fetch_packages.py build/deps/msys2-ucrt64 openssl libsodium opus rnnoise
cmake -S . -B build/windows-cross -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
cmake --build build/windows-cross
ctest --test-dir build/windows-cross --output-on-failure
```

### Output

Every build assembles a self-contained folder under `build/windows-cross/dist/`:

```text
dist/ts-cli/ts-cli.exe          plus the DLLs it imports (OpenSSL, libsodium, Opus, RNNoise, C++ runtime), about 18 MB
```

The folder is relocatable and runs as-is on a bare 64-bit Windows machine; zip it to distribute (`make release` does). [`cmake/bundle_windows.cmake`](cmake/bundle_windows.cmake) assembles them by walking each executable's DLL imports, so it contains exactly what the program needs. The executable in `build/windows-cross/client/` itself only run with the MSYS2 DLLs on the search path; `ctest` arranges that through `WINEPATH`.

### Running it under Wine

```bash
wine build/windows-cross/dist/ts-cli/ts-cli.exe voice.example.net:9987
```

Console coloring and cursor control on Windows use the native Console API rather than ANSI escape sequences, so this renders correctly both under Wine and on real Windows consoles.

### Current limitations

Audio on Windows uses a real WASAPI backend (shared-mode, event-driven capture and render, device enumeration and switching) — see `audio/src/win32/wasapi_backend.cpp`. A few things to know:

* RNNoise noise suppression is available on Windows too (`/audio filter rnnoise`);
* exclusive mode and live device hot-plug notifications aren't implemented — device changes while the client is running need `/audio input`/`/audio output` to pick up the new device, rather than switching automatically;
* this backend has been exercised on real Windows hardware — capture and playback behave as they do on PipeWire — as well as under Wine (device enumeration, capture/render start/stop, device switching, and error rejection for an invalid device). If you do hit a glitch or a device that won't enumerate correctly, `audio/src/win32/wasapi_backend.cpp` is the place to look, and `audio/include/audio/audio_backend.hpp` is the interface it implements.

### Toolchain internals

The toolchain file at [`cmake/toolchains/mingw-w64.cmake`](cmake/toolchains/mingw-w64.cmake) documents the non-obvious parts directly:

* how the fetched MSYS2 prefix becomes the pkg-config sysroot (MSYS2's `.pc` files say `prefix=/ucrt64`), and why both it and the toolchain's own sysroot are find roots (the latter provides the Win32 import libraries such as `gdi32`);
* how `try_compile()` projects get the same prefix (`CMAKE_TRY_COMPILE_PLATFORM_VARIABLES`);
* how `ctest` finds the DLLs under Wine (`WINEPATH` in the emulator command).

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
     mingw-w64-ucrt-x86_64-rnnoise \
     make
   ```

   `mingw-w64-ucrt-x86_64-clang-tools-extra` provides `clang-format` (used by `make format`, which every Makefile target runs first — see *Build* below). The plain `make` package (not `mingw-w64-ucrt-x86_64-make`, which installs as `mingw32-make` instead) gives you the literal `make` command needed to drive the Makefile. RNNoise is optional, same as on Linux: leave out `mingw-w64-ucrt-x86_64-rnnoise` and the build degrades gracefully (`/audio filter rnnoise` just stays unavailable).

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
  -DTS_WINDOWS_RUNTIME_PREFIX="$(cygpath -m "$MINGW_PREFIX")"
cmake --build build/windows-msys
ctest --test-dir build/windows-msys --output-on-failure
```

The `env -u ...` prefix keeps a stray `CMAKE_TOOLCHAIN_FILE`/`PKG_CONFIG_*` setting left over from a Linux cross-build shell from leaking into this native build. `TS_WINDOWS_RUNTIME_PREFIX` names the environment's own UCRT64 prefix, which the build copies the program's DLLs from.

The resulting folder is `build/windows-msys/dist/ts-cli/`, laid out exactly like the [cross-compiled output](#output) and identical in behavior: `WIN32`/`MINGW` are set by CMake based on the compiler's target, not by which machine is doing the compiling, so it's the same WASAPI backend and the same networking/protocol/CLI code, with no path-specific branching anywhere.

> [!NOTE]
> This path hasn't been exercised by the person who wrote it — the reference environment for this project cross-compiles from Linux under Wine, not a native Windows/MSYS2 box. If you hit an MSYS2-specific packaging quirk (a misplaced `.pc` file, a static-vs-import-library mismatch), pointing CMake straight at the real paths is the likely fix. Please report back what you find.

---

## Troubleshooting

* **`No Windows libraries at .../build/deps/msys2-ucrt64/ucrt64` at configure time** — the MSYS2 packages haven't been fetched. Run `make windows-cross`, or the `fetch_packages.py` command shown under *Build*.
* **A test or `.exe` under `build/windows-cross/client/` fails to start with a missing DLL** — run the copy in `build/windows-cross/dist/` instead, or put `build/deps/msys2-ucrt64/ucrt64/bin` on `WINEPATH`.
* **`windns.h` / `VOID` / `ULONG` compile errors on Windows sources** — `windows.h` must be included before `windns.h`. This is already handled in-tree with `clang-format off` guards around the affected includes; if you hit this in new code, do the same.
* **`wine: command not found` while running `make windows-cross`** — Wine wasn't found, so cross-compiled tests can't run automatically; the configure step will warn but still produce `build/windows-cross/dist/ts-cli/`, which you can copy to a real Windows machine instead.

---

## Next steps

Once you can build it, see [CONTRIBUTING.md](CONTRIBUTING.md) for code style, project boundaries, and the platform-backend pattern to follow when extending Linux or Windows support.
