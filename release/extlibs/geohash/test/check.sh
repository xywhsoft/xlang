#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-geohash-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[geohash] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
       -Irelease/extlibs/geohash/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/geohash release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_geohash.xl release/examples/extlibs/geohash.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
# 与 Windows 门禁一致，只规范化黄金文件行尾，不放宽正文比较。
golden="$work/geohash.expected.txt"
sed 's/\r$//' release/extlibs/geohash/examples/geohash.expected.txt > "$golden"

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/geohash/include -DXRT_MODULE_STRING \
    -DXRT_MODULE_STRING_FORMAT -DXRT_MODULE_NUMBER_FLOAT \
    release/extlibs/geohash/geohash_native.c

"$cc" "${flags[@]}" -c release/extlibs/geohash/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/geohash/geohash_native.c \
    release/extlibs/geohash/test/test_geohash.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_geohash"
timeout 90s "$work/test_geohash"

"$cc" "${flags[@]}" release/extlibs/geohash/geohash_native.c \
    release/extlibs/geohash/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads" | tee "$work/threads.txt"
printf 'geohash threads: PASS\n' | diff -u - "$work/threads.txt"

"$cc" "${flags[@]}" release/extlibs/geohash/geohash_native.c \
    release/extlibs/geohash/examples/geohash_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/geohash_example"
timeout 30s "$work/geohash_example" > "$work/c.txt"
diff -u "$golden" "$work/c.txt"
timeout 30s ./release/xl run release/extlibs/geohash/examples/geohash_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_geohash.xl
timeout 90s ./release/xl run release/extlibs/geohash/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/geohash/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/geohash/test/owned_results.xl
timeout 90s ./release/xl cgen release/extlibs/geohash/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in encode bbox neighbor; do
    expected=1
    if [[ $symbol == neighbor ]]; then expected=4; fi
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xgeohash_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != "$expected" ]]; then
        printf 'geohash: return probe must cover %s %s times\n' "$symbol" "$expected" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'geohash: adopted text was copied again\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/geohash.xl > "$work/xl.txt"
diff -u "$golden" "$work/xl.txt"
timeout 120s ./release/xl build test/cases/extlib_geohash.xl \
    -o "$work/geohash_core_aot"
timeout 45s "$work/geohash_core_aot" > "$work/core_aot.txt"
test ! -s "$work/core_aot.txt"
timeout 120s ./release/xl build release/extlibs/geohash/test/owned_results.xl \
    -o "$work/geohash_owned_aot"
timeout 45s "$work/geohash_owned_aot"
timeout 120s ./release/xl build release/extlibs/geohash/test/boundaries.xl \
    -o "$work/geohash_boundaries_aot"
timeout 45s "$work/geohash_boundaries_aot" > "$work/boundaries_aot.txt"
printf 'geohash boundaries: PASS\n' | diff -u - "$work/boundaries_aot.txt"
timeout 120s ./release/xl build release/extlibs/geohash/test/allocations.xl \
    -o "$work/geohash_allocations_aot"
timeout 45s "$work/geohash_allocations_aot"
timeout 120s ./release/xl build release/examples/extlibs/geohash.xl \
    -o "$work/geohash_example_xl_aot"
timeout 45s "$work/geohash_example_xl_aot" > "$work/xl_aot.txt"
diff -u "$golden" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/extlibs/geohash/examples/geohash_c.c \
    -o "$work/geohash_example_c_aot"
timeout 45s "$work/geohash_example_c_aot" > "$work/c_aot.txt"
diff -u "$golden" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'geohash Linux C ASan/UBSan and XLang parity gate: PASS\n'
