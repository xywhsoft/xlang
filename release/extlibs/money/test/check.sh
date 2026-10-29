#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-money-XXXXXX")
pkg=release/extlibs/money
cc=${CC:-gcc}; python=${PYTHON:-python3}; phases=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxlc.so test/cases/extlib_money.xl \
        test/cases/extlib_money_boundaries.xl release/examples/extlibs/money.xl
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
    else printf '[money] FAIL exit=%s artifacts=%s\n' "$status" "$work" >&2; fi
    printf '[money] report: %s.report.txt\n' "$work"
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
run 10 "$python" -c 'import json,sys; assert sorted(json.load(open("release/extlibs/money/xlang.package.json"))["artifact"]["xrtModules"])==["XRT_MODULE_STRING","XRT_MODULE_VALUE_CONTAINER"]; print(sys.version)'
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
compile -DXRT_IMPLEMENTATION "$pkg/examples/settlement_c.c" -no-pie -lm -ldl -o "$work/minimal"
sed 's/\r$//' "$pkg/examples/settlement.expected.txt" > "$work/settlement.expected"
sed 's/\r$//' "$pkg/examples/money.expected.txt" > "$work/legacy.expected"
run 60 "$work/minimal" > "$work/minimal.txt"; diff -u "$work/settlement.expected" "$work/minimal.txt"
compile -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
compile -c "$pkg/money_native.c" -o "$work/native.o"
compile -DNDEBUG "$pkg/test/test_money.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/legacy"
run 120 "$work/legacy"
source="$pkg/test/boxed_records_c.c"
compile -DNDEBUG "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/boxed"
printf 'money boxed: 657 cases, 5 allocation paths, PASS\n' > "$work/boxed.expected"
run 120 "$work/boxed" > "$work/boxed.txt"; diff -u "$work/boxed.expected" "$work/boxed.txt"
run 120 "$work/boxed" --records > "$work/gcc.tsv"
run 60 "$python" "$pkg/test/check_boxed_records.py" "$work/gcc.tsv"
run 120 ./release/xl run "$source" -- --records > "$work/direct.tsv"; cmp "$work/gcc.tsv" "$work/direct.tsv"
run 60 "$python" "$pkg/test/check_boxed_records.py" "$work/direct.tsv"
owned="$pkg/test/owned_format.xl"
run 120 ./release/xl run "$owned"
run 120 ./release/xl cgen "$owned" --emit "$work/owned.c"
grep -Eq 'extern char\* xm_format\(' "$work/owned.c"
grep -Eq '\bv[0-9]+\.Data[[:space:]]*=[[:space:]]*xm_format[[:space:]]*\(' "$work/owned.c"
run 120 ./release/xl cgen test/cases/extlib_money.xl --emit "$work/all.c"
grep -E '^extern .*\bxm_[a-z_]+[(]' "$work/all.c" | sort -u > "$work/declarations.inc"
grep -oE 'xm_[a-z_]+[(]' "$work/declarations.inc" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xm_[a-z_]+)".*/\1/p' "$pkg/src/money.xl" | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/declarations.inc")" -eq 9; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "money_native.h"\n' > "$work/abi.c"; cat "$work/declarations.inc" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for file in "$pkg/examples/money.c" "$pkg/examples/settlement_c.c"; do
    name=$(basename "$file" .c); expected="$work/legacy.expected"
    if [[ $name == settlement_c ]]; then expected="$work/settlement.expected"; fi
    compile -DNDEBUG "$file" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"; diff -u "$expected" "$work/$name.txt"
    run 120 ./release/xl run "$file" > "$work/$name-direct.txt"; diff -u "$expected" "$work/$name-direct.txt"
done
language=(test/cases/extlib_money.xl test/cases/extlib_money_boundaries.xl "$pkg/test/error_kinds.xl"
    "$pkg/examples/settlement.xl" release/examples/extlibs/money.xl)
for file in "${language[@]}"; do
    run 120 ./release/xl run "$file" > "$work/language.txt"
    if [[ $file == */settlement.xl ]]; then diff -u "$work/settlement.expected" "$work/language.txt"; fi
    if [[ $file == release/examples/extlibs/money.xl ]]; then diff -u "$work/legacy.expected" "$work/language.txt"; fi
done
for file in "$source" "$owned" "$pkg/test/aot_core.xl" "$pkg/examples/money.c" "$pkg/examples/settlement_c.c" "${language[@]}"; do
    run 120 ./release/xl build "$file" -o "$work/aot"; run 120 "$work/aot" > "$work/aot.txt"
    case "$file" in
        */boxed_records_c.c)
            diff -u "$work/boxed.expected" "$work/aot.txt"
            run 120 "$work/aot" --records > "$work/aot.tsv"; cmp "$work/gcc.tsv" "$work/aot.tsv"
            run 60 "$python" "$pkg/test/check_boxed_records.py" "$work/aot.tsv" ;;
        */settlement*) diff -u "$work/settlement.expected" "$work/aot.txt" ;;
        */money.c|release/examples/extlibs/money.xl) diff -u "$work/legacy.expected" "$work/aot.txt" ;;
        "$owned"|*/aot_core.xl) test ! -s "$work/aot.txt" ;;
    esac
done
# 外来数组相等性是原正例，不能改成逐元素绕过。
run 120 ./release/xl run "$pkg/test/allocations.xl"
run 120 ./release/xl build "$pkg/test/allocations.xl" -o "$work/allocations"; run 120 "$work/allocations"
printf 'money complete C/XLang gate: PASS\n'
