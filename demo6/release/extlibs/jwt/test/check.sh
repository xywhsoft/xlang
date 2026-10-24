#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../../../.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
cd "$root"

cc -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Ilib/xrt \
    release/extlibs/jwt/test/test_jwt.c \
    release/extlibs/netrc/test/xrt_runtime.c \
    -lpthread -ldl -lm -o "$work/jwt_test"
ASAN_OPTIONS=detect_leaks=1 "$work/jwt_test"
