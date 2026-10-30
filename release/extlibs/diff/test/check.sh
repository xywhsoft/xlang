#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-diff-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[diff] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/diff -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/diff release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_diff.xl release/examples/extlibs/diff.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
golden="$work/diff.expected.txt"
sed 's/\r$//' release/extlibs/diff/test/diff.expected.txt > "$golden"

"$cc" "${flags[@]}" -c release/extlibs/diff/test/runtime.c -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/test/test_diff.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_diff"
timeout 90s "$work/test_diff"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    -DNDEBUG release/extlibs/diff/test/test_unterminated.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_unterminated"
timeout 90s "$work/test_unterminated"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/diff/diff_native.c \
    release/extlibs/diff/examples/diff_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/diff_example"
timeout 30s "$work/diff_example" > "$work/c_example.txt"
diff -u "$golden" "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/diff/examples/diff_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_diff.xl
timeout 90s ./release/xl run release/extlibs/diff/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/diff/test/owned_results.xl
timeout 90s ./release/xl cgen release/extlibs/diff/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in lines stats; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xdiff_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'diff probe must directly adopt %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'diff copied validated input or adopted result text\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/diff.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
for name in central owned_results; do
    source="release/extlibs/diff/test/$name.xl"
    if [[ $name == central ]]; then source=test/cases/extlib_diff.xl; fi
    timeout 120s ./release/xl build "$source" -o "$work/diff_$name"
    timeout 45s "$work/diff_$name"
done
timeout 120s ./release/xl build release/extlibs/diff/test/boundaries.xl \
    -o "$work/diff_boundaries_aot"
timeout 45s "$work/diff_boundaries_aot" > "$work/boundaries_aot.txt"
grep -Fxq 'diff boundaries: PASS' "$work/boundaries_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/diff.xl \
    -o "$work/diff_example_xl_aot"
timeout 45s "$work/diff_example_xl_aot" > "$work/xl_example_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_example_aot.txt"
timeout 120s ./release/xl build release/extlibs/diff/examples/diff_c.c \
    -o "$work/diff_example_c_aot"
timeout 45s "$work/diff_example_c_aot" > "$work/c_example_aot.txt"
diff -u "$work/c_example.txt" "$work/c_example_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'diff Linux C ASan/UBSan and XLang parity gate: PASS\n'
