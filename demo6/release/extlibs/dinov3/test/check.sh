#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
BUILD_ROOT=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
BUILD=$(mktemp -d "$BUILD_ROOT/xlang-dinov3-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$BUILD" in "$BUILD_ROOT"/xlang-dinov3-*) rm -rf -- "$BUILD";; *) exit 1;; esac
    else
        printf 'DinoV3 failure artifacts retained: %s\n' "$BUILD" >&2
    fi
}
trap cleanup EXIT
run() {
    label=$1
    shift
    printf '\n[%s] ' "$label" >> "$BUILD/commands.log"
    printf '%s ' "$@" >> "$BUILD/commands.log"
    printf '\n' >> "$BUILD/commands.log"
    if "$@" >> "$BUILD/commands.log" 2>&1; then
        printf '[dinov3] %s: PASS\n' "$label"
    else
        cat "$BUILD/commands.log" >&2
        return 1
    fi
}
find "$ROOT/release/extlibs/dinov3" -type f -exec sha256sum {} + > "$BUILD/inputs.sha256"
sha256sum "$ROOT/lib/xrt/xrt.h" "$ROOT/release/xl" >> "$BUILD/inputs.sha256"
run 'C compiler version' "${CC:-cc}" --version
run 'C sanitizer build' "${CC:-cc}" -std=c11 -D_POSIX_C_SOURCE=200809L -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_ONCE -DXRT_MODULE_THREAD_KEY \
    -I"$ROOT/lib/xrt" -I"$ROOT/release/extlibs/dinov3/include" \
    "$ROOT/release/extlibs/dinov3/test/test_dinov3.c" \
    "$ROOT/release/extlibs/dinov3/src/dinov3_artifact.c" \
    "$ROOT/release/extlibs/dinov3/test/fake_ort.c" \
    "$ROOT/release/extlibs/dinov3/test/runtime.c" \
    -ldl -lm -pthread -o "$BUILD/test_dinov3_linux"
run 'C result processing and allocation budgets' "$BUILD/test_dinov3_linux"
cd "$ROOT"
run 'XLang name boundaries' timeout 90s ./release/xl run release/extlibs/dinov3/test/nul_names.xl
run 'XLang unavailable output' timeout 90s ./release/xl run release/extlibs/dinov3/test/invalid_output.xl
sha256sum -c "$BUILD/inputs.sha256" > "$BUILD/inputs.recheck"
printf '[dinov3] Linux C sanitizer and XLang negative boundaries: PASS\n'
