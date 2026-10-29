#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-geohash-XXXXXX")
pkg=release/extlibs/geohash
cc=${CC:-gcc}; python=${PYTHON:-python3}; phases=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
paths() {
    find "$pkg" release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_geohash.xl release/examples/extlibs/geohash.xl
    if [[ -n $cc_path ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n $python_path ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    status=$?; trap - EXIT; set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then status=125; fi
    printf 'exit=%s\nphases=%s\n' "$status" "$phases" > "$work.report.txt"
    if [[ -f $work/commands.log ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((status==0)); then rm -rf -- "$work"
    else printf '[geohash] FAIL exit=%s artifacts=%s\n' "$status" "$work" >&2; fi
    printf '[geohash] report: %s.report.txt\n' "$work"
    exit "$status"
}
trap cleanup EXIT
mapfile -d '' before < <(paths | sort -zu)
sha256sum "${before[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 status; shift; phases=$((phases+1))
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"; printf ' %q' "$@" >> "$work/commands.log"; printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$phases.out" 2> "$work/$phases.err"; then status=0; else status=$?; fi
    printf 'exit=%s\n' "$status" >> "$work/commands.log"; cat "$work/$phases.out" "$work/$phases.err" >> "$work/commands.log"
    cat "$work/$phases.out"; cat "$work/$phases.err" >&2
    return "$status"
}
export PYTHONDONTWRITEBYTECODE=1 XLANG_PACKAGE_PATH="$root/release/extlibs"
run 10 "$cc" --version
run 10 "$python" -c 'import json,sys; assert sorted(json.load(open("release/extlibs/geohash/xlang.package.json"))["artifact"]["xrtModules"])==["XRT_MODULE_NUMBER_FLOAT","XRT_MODULE_STRING","XRT_MODULE_STRING_FORMAT"]; print(sys.version)'
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
compile -DXRT_IMPLEMENTATION "$pkg/examples/dateline_c.c" -no-pie -lm -ldl -o "$work/minimal"
sed 's/\r$//' "$pkg/examples/dateline.expected.txt" > "$work/dateline.expected"
sed 's/\r$//' "$pkg/examples/geohash.expected.txt" > "$work/legacy.expected"
run 60 "$work/minimal" > "$work/minimal.txt"; diff -u "$work/dateline.expected" "$work/minimal.txt"
compile -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
compile -c "$pkg/geohash_native.c" -o "$work/native.o"
for name in test_geohash test_threads; do
    compile -DNDEBUG "$pkg/test/$name.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"
done
printf 'geohash threads: PASS\n' > "$work/threads.expected"; diff -u "$work/threads.expected" "$work/test_threads.txt"
source="$pkg/test/grid_records_c.c"
compile -DNDEBUG "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/grid"
printf 'geohash grid: 2696 records, PASS\n' > "$work/grid.expected"
run 120 "$work/grid" > "$work/grid.txt"; diff -u "$work/grid.expected" "$work/grid.txt"
run 120 "$work/grid" --records > "$work/gcc.tsv"
run 60 "$python" "$pkg/test/check_grid_records.py" "$work/gcc.tsv"
run 120 ./release/xl run "$source" -- --records > "$work/direct.tsv"; cmp "$work/gcc.tsv" "$work/direct.tsv"
run 60 "$python" "$pkg/test/check_grid_records.py" "$work/direct.tsv"
run 120 ./release/xl cgen "$pkg/test/owned_probe.xl" --emit "$work/owned.c"
for symbol in encode bbox neighbor; do
    expected=1; if [[ $symbol == neighbor ]]; then expected=4; fi
    test "$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xgeohash_${symbol}[[:space:]]*\(" "$work/owned.c")" = "$expected"
done
if grep -Eq 'xl6_xrt_take_cstr\(|xl6CStringCopyV1|= *xlir_string_copy *\(' "$work/owned.c"; then echo 'Adopted text copied again' >&2; exit 1; fi
run 120 ./release/xl cgen test/cases/extlib_geohash.xl --emit "$work/all.c"
grep -E '^extern .*\bxgeohash_[a-z_]+[(]' "$work/all.c" | sort -u > "$work/declarations.inc"
grep -oE 'xgeohash_[a-z_]+[(]' "$work/declarations.inc" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xgeohash_[a-z_]+)".*/\1/p' "$pkg/src/geohash.xl" | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/declarations.inc")" -eq 6; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "geohash_native.h"\n' > "$work/abi.c"; cat "$work/declarations.inc" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for file in "$pkg/examples/geohash_c.c" "$pkg/examples/dateline_c.c"; do
    name=$(basename "$file" .c); expected="$work/legacy.expected"; extra=("$work/native.o")
    if [[ $name == dateline_c ]]; then expected="$work/dateline.expected"; extra=(); fi
    compile -DNDEBUG "$file" "${extra[@]}" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"; diff -u "$expected" "$work/$name.txt"
    run 120 ./release/xl run "$file" > "$work/$name-direct.txt"; diff -u "$expected" "$work/$name-direct.txt"
done
language=(test/cases/extlib_geohash.xl "$pkg/test/boundaries.xl" "$pkg/test/allocations.xl" "$pkg/test/owned_results.xl"
    "$pkg/test/owned_probe.xl" release/examples/extlibs/geohash.xl "$pkg/examples/dateline.xl")
index=0
for file in "${language[@]}"; do
    index=$((index+1)); run 120 ./release/xl run "$file" > "$work/language-$index.txt"
    case "$file" in
        */dateline.xl) diff -u "$work/dateline.expected" "$work/language-$index.txt" ;;
        release/examples/extlibs/geohash.xl) diff -u "$work/legacy.expected" "$work/language-$index.txt" ;;
        */boundaries.xl) printf 'geohash boundaries: PASS\n' | diff -u - "$work/language-$index.txt" ;;
        test/cases/extlib_geohash.xl) test ! -s "$work/language-$index.txt" ;;
    esac
done
index=0
for file in "$source" "$pkg/examples/geohash_c.c" "$pkg/examples/dateline_c.c" "${language[@]}"; do
    index=$((index+1)); run 120 ./release/xl build "$file" -o "$work/aot"; run 120 "$work/aot" > "$work/aot.txt"
    case "$file" in
        */grid_records_c.c)
            diff -u "$work/grid.expected" "$work/aot.txt"
            run 120 "$work/aot" --records > "$work/aot.tsv"; cmp "$work/gcc.tsv" "$work/aot.tsv"
            run 60 "$python" "$pkg/test/check_grid_records.py" "$work/aot.tsv" ;;
        */dateline_c.c) diff -u "$work/dateline.expected" "$work/aot.txt" ;;
        */geohash_c.c) diff -u "$work/legacy.expected" "$work/aot.txt" ;;
        *) diff -u "$work/language-$((index-3)).txt" "$work/aot.txt" ;;
    esac
done
printf 'geohash complete C/XLang gate: PASS\n'
