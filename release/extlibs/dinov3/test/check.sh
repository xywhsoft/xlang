#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/xlang-dinov3-XXXXXX")
trap 'rm -rf -- "$BUILD"' EXIT
"${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_ONCE -DXRT_MODULE_THREAD_KEY \
    -I"$ROOT/lib/xrt" -I"$ROOT/release/extlibs/dinov3/include" \
    "$ROOT/release/extlibs/dinov3/test/test_dinov3.c" \
    "$ROOT/release/extlibs/dinov3/src/dinov3_artifact.c" \
    "$ROOT/release/extlibs/dinov3/test/fake_ort.c" \
    "$ROOT/release/extlibs/dinov3/test/runtime.c" \
    -ldl -lm -pthread -o "$BUILD/test_dinov3_linux"
"$BUILD/test_dinov3_linux"
(cd "$ROOT" && timeout 90s ./release/xl run release/extlibs/dinov3/test/nul_names.xl)
(cd "$ROOT" && timeout 90s ./release/xl run release/extlibs/dinov3/test/invalid_output.xl)
