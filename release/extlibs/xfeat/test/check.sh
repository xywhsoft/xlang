#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
WORK_ROOT=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
work=$(mktemp -d "$WORK_ROOT/xlang-xfeat-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$work" in "$WORK_ROOT"/xlang-xfeat-*) rm -rf -- "$work";; *) exit 1;; esac
    else
        printf 'XFeat failure artifacts retained: %s\n' "$work" >&2
    fi
}
trap cleanup EXIT
run() {
    label=$1
    shift
    printf '\n[%s] ' "$label" >> "$work/commands.log"
    printf '%s ' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if "$@" >> "$work/commands.log" 2>&1; then
        printf '[xfeat] %s: PASS\n' "$label"
    else
        cat "$work/commands.log" >&2
        return 1
    fi
}
find "$ROOT/release/extlibs/xfeat" "$ROOT/release/extlibs/opencv" \
    "$ROOT/release/extlibs/onnxruntime" "$ROOT/release/include" \
    -type f -exec sha256sum {} + > "$work/inputs.sha256"
sha256sum "$ROOT/lib/xrt/xrt.h" "$ROOT/lib/xruntime/xruntime.h" \
    "$ROOT/release/xl" "$ROOT/release/libxl.so" "$ROOT/release/libxlc.so" \
    >> "$work/inputs.sha256"
mkdir -p "$work/中文路径"
run 'C compiler version' "${CC:-cc}" --version
run 'matching sanitizer build' "${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_PATH_SYSTEM -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_ONCE \
    -DXRT_MODULE_MUTEX -DXRT_MODULE_THREAD_KEY \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -I"$ROOT/lib/xrt" \
    "$ROOT/release/extlibs/xfeat/test/test_match.c" \
    "$ROOT/release/extlibs/xfeat/test/runtime.c" -ldl -lm -pthread \
    -o "$work/test_match_linux"
run 'matching core' "$work/test_match_linux"
run 'fake bridge build' "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -shared -fPIC \
    "$ROOT/release/extlibs/xfeat/test/fake_ort.c" \
    -o "$work/中文路径/fake_ort.so"
run 'loader sanitizer build' "${CC:-cc}" -std=c11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXRT_MODULE_PATH_SYSTEM -DXRT_MODULE_ENVIRONMENT -DXRT_MODULE_ONCE \
    -DXRT_MODULE_MUTEX -DXRT_MODULE_THREAD_KEY \
    -DXRT_MODULE_VALUE_CONTAINER -DXRT_MODULE_JSON -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_MEMORY_DEBUG \
    -I"$ROOT/lib/xrt" -I"$ROOT/release/extlibs/xfeat/include" \
    "$ROOT/release/extlibs/xfeat/test/test_loader.c" \
    "$ROOT/release/extlibs/xfeat/src/xfeat_artifact.c" \
    "$ROOT/release/extlibs/xfeat/test/runtime.c" -ldl -lm -pthread \
    -o "$work/test_loader_linux"
run 'UTF-8 bridge, rows, and owned budgets' env ONNXRUNTIME_XLANG_DLL="$work/中文路径/fake_ort.so" \
    "$work/test_loader_linux"
cd "$ROOT"
run 'XLang no-model core' timeout 90s ./release/xl run release/extlibs/xfeat/test/aot_core.xl
run 'XLang JSON failures' timeout 90s ./release/xl run release/extlibs/xfeat/test/json_failure.xl
run 'XLang owned budgets' timeout 90s ./release/xl run release/extlibs/xfeat/test/owned_results.xl
run 'XLang NUL arguments' timeout 90s ./release/xl run release/extlibs/xfeat/test/nul_arguments.xl
run 'XLang catalog contract' timeout 90s ./release/xl run test/cases/extlib_xfeat.xl
run 'XLang owned AOT build' timeout 90s ./release/xl build release/extlibs/xfeat/test/owned_results.xl -o "$work/owned_results"
run 'XLang owned AOT' timeout 90s "$work/owned_results"
sha256sum -c "$work/inputs.sha256" > "$work/inputs.recheck"
printf '[xfeat] Linux C sanitizer and no-model language checks: PASS\n'
