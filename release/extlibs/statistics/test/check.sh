#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-statistics-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/statistics release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_statistics.xl release/examples/extlibs/statistics.xl
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\nphases=%s\n' "$result" "$sequence" > "$work.report.txt"
    if [[ -f "$work/commands.log" ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[statistics] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[statistics] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 result
    shift
    sequence=$((sequence+1))
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"
    printf ' %q' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"
    cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"
    cat "$work/$sequence.err" >&2
    return "$result"
}
run 10 "$cc" --version > "$work/compiler.txt"
run 10 "$python" --version
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/statistics
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/statistics/statistics_native.h
compile -DXRT_MODULE_VALUE_CONTAINER -fsyntax-only release/extlibs/statistics/statistics_native.c
compile -fsyntax-only release/extlibs/statistics/test/abi_contract.c
compile -c release/extlibs/statistics/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/statistics/statistics_native.c -o "$work/native.o"
for name in test_statistics size_guard translation_c; do
    objects=("$work/runtime.o")
    if [[ $name != size_guard ]]; then objects+=("$work/native.o"); fi
    compile -DNDEBUG "release/extlibs/statistics/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'statistics translation: 160 checks, 0 failures\n' > "$work/translation.expected"
diff -u "$work/translation.expected" "$work/translation_c.txt"
run 90 "$work/translation_c" --records > "$work/gcc.tsv"
run 60 "$python" release/extlibs/statistics/test/check_translation.py "$work/gcc.tsv"
run 90 ./release/xl run release/extlibs/statistics/test/translation_c.c > "$work/translation.direct"
diff -u "$work/translation.expected" "$work/translation.direct"
names=(statistics_c trend_c distribution_c measurement_offsets_c)
languages=(basic trend distribution measurement_offsets)
for index in "${!names[@]}"; do
    name=${names[$index]}; language=${languages[$index]}
    objects=("$work/runtime.o")
    if [[ $name == measurement_offsets_c ]]; then objects+=("$work/native.o"); fi
    compile -DNDEBUG "release/extlibs/statistics/examples/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    sed 's/\r$//' "release/extlibs/statistics/examples/$language.expected.txt" > "$work/$name.expected"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "release/extlibs/statistics/examples/$name.c" > "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
done
run 90 ./release/xl cgen release/extlibs/statistics/examples/measurement_offsets.xl --emit "$work/probe.c"
grep -E '^extern .*xstat_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xstat_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xstat_[a-z_]+)".*/\1/p' release/extlibs/statistics/src/statistics.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 23
test "$(wc -l < "$work/abi.expected")" -eq 23
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "statistics_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for source in release/extlibs/statistics/test/translation_c.c release/extlibs/statistics/test/size_guard.c \
    release/extlibs/statistics/examples/statistics_c.c release/extlibs/statistics/examples/trend_c.c \
    release/extlibs/statistics/examples/distribution_c.c release/extlibs/statistics/examples/measurement_offsets_c.c; do
    name=$(basename "${source%.*}")
    target="$work/$name-aot"
    run 180 ./release/xl build "$source" -o "$target"
    run 120 "$target" > "$target.out"
    if [[ -f "$work/$name.expected" ]]; then diff -u "$work/$name.expected" "$target.out"; fi
    if [[ $name == translation_c ]]; then
        diff -u "$work/translation.expected" "$target.out"
        run 90 "$target" --records > "$work/tcc.tsv"
        run 60 "$python" release/extlibs/statistics/test/check_translation.py "$work/tcc.tsv"
    fi
done
# 正常语言数组仍为必过路径，不用C工厂制造一条替代调用来绕过M1。
for source in release/extlibs/statistics/examples/measurement_offsets.xl test/cases/extlib_statistics.xl \
    release/extlibs/statistics/test/boundaries.xl release/extlibs/statistics/test/allocations.xl \
    release/extlibs/statistics/examples/basic.xl release/extlibs/statistics/examples/trend.xl \
    release/extlibs/statistics/examples/distribution.xl release/examples/extlibs/statistics.xl; do
    run 120 ./release/xl run "$source" > "$work/full.txt"
    for index in "${!languages[@]}"; do
        if [[ $source == "release/extlibs/statistics/examples/${languages[$index]}.xl" ]]; then
            diff -u "$work/${names[$index]}.expected" "$work/full.txt"
        fi
    done
    target="$work/$(basename "${source%.*}")-full"
    run 180 ./release/xl build "$source" -o "$target"
    run 120 "$target" > "$target.out"
    for index in "${!languages[@]}"; do
        if [[ $source == "release/extlibs/statistics/examples/${languages[$index]}.xl" ]]; then
            diff -u "$work/${names[$index]}.expected" "$target.out"
        fi
    done
done
printf 'statistics complete C/XLang/AOT gate: PASS\n'
