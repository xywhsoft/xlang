#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
BUILD="$ROOT/build/test/op"
mkdir -p "$BUILD"
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$ROOT/release/extlibs/op/test/test_hook_bounds.c" \
    -o "$BUILD/hook_bounds_linux"
"$BUILD/hook_bounds_linux"
