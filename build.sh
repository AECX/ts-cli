#!/usr/bin/env bash

set -euo pipefail

./format.sh

cmake -S . -B build
cmake --build build

ctest \
  --test-dir build \
  --output-on-failure
