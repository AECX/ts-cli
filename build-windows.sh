#!/usr/bin/env bash

set -euo pipefail

# Cross-compiles ts-cli for 64-bit Windows via MinGW-w64 and runs the test
# suite under Wine. See cmake/toolchains/mingw-w64.cmake for requirements.
#
# Source formatting already runs against the same sources via ./build.sh;
# no need to repeat it for a second target.

cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake
cmake --build build-windows

ctest \
  --test-dir build-windows \
  --output-on-failure
