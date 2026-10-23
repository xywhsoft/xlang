#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
BUILD_PARENT="$ROOT/build/test/op"
mkdir -p "$BUILD_PARENT"
BUILD=$(mktemp -d "$BUILD_PARENT/run_XXXXXX")
case "$BUILD" in "$BUILD_PARENT"/run_*) ;; *) exit 1 ;; esac
trap 'rm -rf -- "$BUILD"' EXIT
"${CC:-cc}" -std=c11 -O1 -g -DNDEBUG -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$ROOT/release/extlibs/op/test/test_hook_bounds.c" \
    -o "$BUILD/hook_bounds_linux"
"$BUILD/hook_bounds_linux"
