#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-json5-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[json5] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
pkg=release/extlibs/json5
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -I"$pkg" -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find "$pkg" -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/cases/extlib_json5.xl \
          release/examples/extlibs/json5.xl
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"

# 数值/深度是独立的边界，三个 C 测试共用同一组严格编译的对象。
timeout 120s "$cc" "${flags[@]}" -c "$pkg/test/xrt_runtime.c" -o "$work/xrt_runtime.o"
timeout 120s "$cc" "${flags[@]}" -c "$pkg/json5_native.c" -o "$work/native.o"
timeout 120s "$cc" "${flags[@]}" -c "$pkg/vendor/pdjson5.c" -o "$work/parser.o"
objects=("$work/native.o" "$work/parser.o" "$work/xrt_runtime.o")
for name in boundaries numbers depth; do
    "$cc" "${flags[@]}" "$pkg/test/test_$name.c" "${objects[@]}" \
        -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
timeout 120s "$cc" "${flags[@]}" "$pkg/test/test_unicode_escapes.c" "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/unicode"
timeout 90s "$work/unicode"
timeout 90s release/xl run "$pkg/test/test_unicode_escapes.c"
timeout 90s release/xl run "$pkg/test/unicode_escapes.xl"
timeout 120s "$cc" "${flags[@]}" "$pkg/examples/unicode_config_c.c" "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/unicode_example"
timeout 90s "$work/unicode_example" > "$work/unicode_example.txt"
diff -u "$pkg/examples/unicode_config.expected.txt" "$work/unicode_example.txt"
for source in test/test_unicode_escapes.c test/unicode_escapes.xl examples/unicode_config.xl examples/unicode_config_c.c; do
    name=$(basename "$source")
    timeout 90s release/xl run "$pkg/$source" > "$work/$name.jit.txt"
    timeout 120s release/xl build "$pkg/$source" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot" > "$work/$name.aot.txt"
    diff -u "$work/$name.jit.txt" "$work/$name.aot.txt"
    if [[ $source == examples/* ]]; then diff -u "$pkg/examples/unicode_config.expected.txt" "$work/$name.jit.txt"; fi
done
if [[ -n "${JSON5_REFERENCE_DIR:-}" ]]; then
    "$cc" "${flags[@]}" "$pkg/test/reference_probe.c" "${objects[@]}" \
        -lm -ldl -o "$work/reference_probe"
    node "$pkg/test/reference_probe.mjs" "$work/reference_probe" "$JSON5_REFERENCE_DIR"
fi
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
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"

# 保持正确的正例；主线 var 显式转换未修复时，整包门禁必须为红。
timeout 90s "$xl" run "$pkg/test/numeric_consumer.xl"
timeout 120s "$xl" build "$pkg/test/numeric_consumer.xl" -o "$work/json5_numeric_consumer"
timeout 45s "$work/json5_numeric_consumer"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
printf 'json5 Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
