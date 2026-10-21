#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-xfeat-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/中文路径"
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_PATH_SYSTEM -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_ONCE \
    -DXRT_MODULE_MUTEX -DXRT_MODULE_THREAD_KEY \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -I"$ROOT/lib/xrt" \
    "$ROOT/release/extlibs/xfeat/test/test_match.c" \
    "$ROOT/release/extlibs/xfeat/test/runtime.c" -ldl -lm -pthread \
    -o "$work/test_match_linux"
"$work/test_match_linux"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -shared -fPIC \
    "$ROOT/release/extlibs/xfeat/test/fake_ort.c" \
    -o "$work/中文路径/fake_ort.so"
"${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_PATH_SYSTEM -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_ONCE \
    -DXRT_MODULE_MUTEX -DXRT_MODULE_THREAD_KEY \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -I"$ROOT/lib/xrt" -I"$ROOT/release/extlibs/xfeat/include" \
    "$ROOT/release/extlibs/xfeat/test/test_loader.c" \
    "$ROOT/release/extlibs/xfeat/src/xfeat_artifact.c" \
    "$ROOT/release/extlibs/xfeat/test/runtime.c" -ldl -lm -pthread \
    -o "$work/test_loader_linux"
ONNXRUNTIME_XLANG_DLL="$work/中文路径/fake_ort.so" \
    "$work/test_loader_linux"
(cd "$ROOT" && timeout 90s ./release/xl run release/extlibs/xfeat/test/aot_core.xl)
(cd "$ROOT" && timeout 90s ./release/xl run release/extlibs/xfeat/test/nul_arguments.xl)
(cd "$ROOT" && timeout 90s ./release/xl run test/cases/extlib_xfeat.xl)
