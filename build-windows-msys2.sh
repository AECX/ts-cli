#!/usr/bin/env bash

set -euo pipefail

# Builds ts-cli natively for 64-bit Windows using the MSYS2 UCRT64
# environment and runs the test suite directly on Windows.
#
# This is intentionally separate from ./build-windows.sh, which
# cross-compiles from Linux via cmake/toolchains/mingw-w64.cmake and
# runs the test suite through Wine.

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "error: this script must be run from an MSYS2 UCRT64 shell" >&2
  echo "       current MSYSTEM=${MSYSTEM:-<unset>}" >&2
  exit 1
fi

if [[ "${MINGW_PREFIX:-}" != "/ucrt64" ]]; then
  echo "error: expected MINGW_PREFIX=/ucrt64, got ${MINGW_PREFIX:-<unset>}" >&2
  exit 1
fi

required_commands=(
  cmake
  ninja
  pkg-config
  gcc
  g++
  cygpath
)

for command_name in "${required_commands[@]}"; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "error: required command not found: ${command_name}" >&2
    echo "       see BUILDING.md for the required MSYS2 UCRT64 packages" >&2
    exit 1
  fi
done

required_pkgconfig_modules=(
  libsodium
  opus
  openssl
)

for module in "${required_pkgconfig_modules[@]}"; do
  if ! pkg-config --exists "${module}"; then
    echo "error: pkg-config module not found: ${module}" >&2
    echo "       see BUILDING.md for the required MSYS2 UCRT64 packages" >&2
    exit 1
  fi
done

# Do not let settings from the Linux -> Windows cross-build leak into this
# native build. Keep MSYS2's normal PKG_CONFIG_SYSTEM_* variables intact.
unset CMAKE_TOOLCHAIN_FILE
unset PKG_CONFIG_SYSROOT_DIR
unset PKG_CONFIG_LIBDIR
unset PKG_CONFIG_PATH

build_dir="${BUILD_DIR:-build-windows-msys2}"
ucrt_root="$(cygpath -m "${MINGW_PREFIX}")"

cmake \
  -S . \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR="${ucrt_root}" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE

cmake --build "${build_dir}"

ctest \
  --test-dir "${build_dir}" \
  --output-on-failure
