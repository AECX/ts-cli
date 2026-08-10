#!/usr/bin/env bash

find . -type f \( -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i {} +
