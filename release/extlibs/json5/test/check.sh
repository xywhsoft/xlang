#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-json5-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
pkg=release/extlibs/json5
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -I"$pkg" -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c "$pkg/test/xrt_runtime.c" -o "$work/xrt_runtime.o"
sources=("$pkg/json5_native.c" "$pkg/vendor/pdjson5.c")
"$cc" "${flags[@]}" "${sources[@]}" "$pkg/test/test_boundaries.c" \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/boundaries"
timeout 90s "$work/boundaries"
"$cc" "${flags[@]}" "$pkg/examples/json5_c.c" \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/json5_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'json5 Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 30s "$xl" run "$pkg/examples/json5_c.c" > "$work/json5_direct_c.txt"
diff -u "$work/json5_c.txt" "$work/json5_direct_c.txt"
timeout 90s "$xl" run test/cases/extlib_json5.xl
timeout 90s "$xl" run release/extlibs/json5/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/json5/test/allocations.xl
timeout 90s "$xl" run release/extlibs/json5/examples/json5.xl > "$work/json5_xl.txt"
diff -u "$work/json5_c.txt" "$work/json5_xl.txt"
timeout 120s "$xl" build "$pkg/test/aot_core.xl" -o "$work/json5_core"
timeout 45s "$work/json5_core"
timeout 120s "$xl" build "$pkg/test/boundaries.xl" -o "$work/json5_boundaries"
timeout 90s "$work/json5_boundaries"
timeout 120s "$xl" build "$pkg/test/allocations.xl" -o "$work/json5_allocations"
timeout 90s "$work/json5_allocations"
timeout 120s "$xl" build "$pkg/examples/json5.xl" -o "$work/json5_example"
timeout 45s "$work/json5_example" > "$work/json5_aot.txt"
diff -u "$work/json5_c.txt" "$work/json5_aot.txt"
timeout 90s "$xl" run release/examples/extlibs/json5.xl > "$work/json5_public_jit.txt"
timeout 120s "$xl" build release/examples/extlibs/json5.xl \
    -o "$work/json5_public_aot"
timeout 45s "$work/json5_public_aot" > "$work/json5_public_aot.txt"
diff -u "$work/json5_public_jit.txt" "$work/json5_public_aot.txt"
timeout 120s "$xl" build "$pkg/examples/json5_c.c" -o "$work/json5_c_aot"
timeout 45s "$work/json5_c_aot" > "$work/json5_c_aot.txt"
diff -u "$work/json5_c.txt" "$work/json5_c_aot.txt"
printf 'json5 Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
