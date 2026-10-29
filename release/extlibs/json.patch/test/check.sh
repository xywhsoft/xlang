#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-json-patch-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/json.patch/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'json.patch: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi

printf '[json.patch] C sanitizer gate\n'
"$cc" "${flags[@]}" -c release/extlibs/json.patch/test/xrt_runtime.c \
    -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/json.patch/json_patch_native.c \
    -o "$work/native.o"
"$cc" "${flags[@]}" release/extlibs/json.patch/test/test_boundaries.c \
    "$work/native.o" "$work/runtime.o" -lm -ldl -o "$work/boundaries"
"$cc" "${flags[@]}" release/extlibs/json.patch/examples/workflow_c.c \
    "$work/runtime.o" -lm -ldl -o "$work/workflow"
"$cc" "${flags[@]}" release/extlibs/json.patch/examples/basic_c.c \
    "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 90s "$work/boundaries"
timeout 30s "$work/workflow" > "$work/workflow_c.txt"
timeout 30s "$work/basic" > "$work/basic_c.txt"
timeout 30s release/xl run release/extlibs/json.patch/examples/workflow_c.c \
    > "$work/workflow_direct_c.txt"
timeout 30s release/xl run release/extlibs/json.patch/examples/basic_c.c \
    > "$work/basic_direct_c.txt"
diff -u "$work/workflow_c.txt" "$work/workflow_direct_c.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct_c.txt"
test "$(cat "$work/workflow_c.txt")" = 'json.patch workflow: ok'

printf '[json.patch] XLang JIT gate\n'
timeout 90s "$xl" run test/cases/extlib_json_patch.xl
timeout 90s "$xl" run release/extlibs/json.patch/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/json.patch/test/allocations.xl
timeout 90s "$xl" run release/extlibs/json.patch/examples/workflow.xl \
    > "$work/workflow_xl.txt"
timeout 90s "$xl" run release/extlibs/json.patch/examples/basic.xl \
    > "$work/basic_xl.txt"
diff -u "$work/workflow_c.txt" "$work/workflow_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
printf 'json.patch Linux C ASan/UBSan and XLang parity gate: PASS\n'
