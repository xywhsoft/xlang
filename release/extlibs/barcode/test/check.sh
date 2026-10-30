#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-barcode-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[barcode] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/barcode/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/barcode release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_barcode.xl test/cases/extlib_barcode_boundaries.xl
          release/examples/extlibs/barcode.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" "${flags[@]}" -c release/extlibs/barcode/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/barcode/barcode_native.c \
    release/extlibs/barcode/test/test_barcode.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_barcode"
timeout 90s "$work/test_barcode"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_barcode.xl
timeout 90s "$xl" run test/cases/extlib_barcode_boundaries.xl
timeout 90s "$xl" run release/extlibs/barcode/test/recovery.xl
timeout 90s "$xl" run release/extlibs/barcode/test/allocations.xl
timeout 90s "$xl" run release/extlibs/barcode/test/owned_results.xl
timeout 90s "$xl" cgen release/extlibs/barcode/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in svg ascii; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xb_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 2 ]]; then
        printf 'barcode probe must adopt %s in both EAN/UPC entries\n' "$symbol" >&2
        exit 1
    fi
done
copies=$(grep -Ec '= *xlir_string_copy *\(' "$work/owned_probe.c" || true)
if ((copies > 1)); then
    printf 'barcode copied the adopted result text\n' >&2
    exit 1
fi

"$cc" "${flags[@]}" release/extlibs/barcode/barcode_native.c \
    release/extlibs/barcode/examples/barcode.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/barcode_example"
timeout 90s "$work/barcode_example" > "$work/c.out"
timeout 90s "$xl" run release/extlibs/barcode/examples/barcode.c > "$work/direct_c.out"
timeout 90s "$xl" run release/examples/extlibs/barcode.xl > "$work/xlang.out"
expected="$work/barcode.expected.txt"
sed 's/\r$//' release/extlibs/barcode/examples/barcode.expected.txt > "$expected"
diff -u "$expected" "$work/c.out"
diff -u "$expected" "$work/direct_c.out"
diff -u "$expected" "$work/xlang.out"

timeout 90s "$xl" build release/extlibs/barcode/test/aot_core.xl -o "$work/barcode_core"
timeout 30s "$work/barcode_core"
for name in central boundaries recovery allocations owned_results; do
    source="release/extlibs/barcode/test/$name.xl"
    if [[ $name == central ]]; then source=test/cases/extlib_barcode.xl; fi
    if [[ $name == boundaries ]]; then source=test/cases/extlib_barcode_boundaries.xl; fi
    timeout 90s "$xl" build "$source" -o "$work/barcode_$name"
    timeout 30s "$work/barcode_$name"
done
timeout 90s "$xl" build release/examples/extlibs/barcode.xl -o "$work/barcode_xl_aot"
timeout 30s "$work/barcode_xl_aot" > "$work/xlang_aot.out"
diff -u "$expected" "$work/xlang_aot.out"
timeout 90s "$xl" build release/extlibs/barcode/examples/barcode.c -o "$work/barcode_c_aot"
timeout 30s "$work/barcode_c_aot" > "$work/c_aot.out"
diff -u "$expected" "$work/c_aot.out"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'barcode Linux C ASan/UBSan, XLang JIT and bilingual AOT gate: PASS\n'
