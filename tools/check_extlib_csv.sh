#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
own_work=true
if [[ -n "${CSV_WORK_DIR:-}" ]]; then
    work=$CSV_WORK_DIR
    [[ ! -e $work ]] || { printf 'CSV work directory already exists: %s\n' "$work" >&2; exit 2; }
    mkdir -p -- "$work"
    own_work=false
else
    work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-csv-XXXXXX")
fi
pkg=release/extlibs/csv
cc=${CC:-gcc}
xl=release/xl
phase=setup
failures=0
mapfile -d '' inputs < <(
    { find "$pkg" release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_csv_runtime.c \
          tools/check_extlib_csv.sh tools/check_extlib_csv_interop.py release/xl release/libxl.so release/libxlc.so \
          test/cases/extlib_csv.xl release/examples/extlibs/csv.xl \
          release/examples/extlibs/csv_basic.xl release/examples/extlibs/csv_files.xl \
          release/examples/extlibs/csv_basic.expected.txt release/examples/extlibs/csv_files.expected.txt
      compiler=$(command -v "$cc" || true)
      if [[ -f "$compiler" ]]; then printf '%s\0' "$compiler"; fi
      if [[ -n ${CSV_DECLARATIONS:-} ]]; then printf '%s\0' "$CSV_DECLARATIONS"; fi
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
cleanup() {
    result=$?
    trap - EXIT
    set +e
    sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
    diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256" > "$work/input-changes.txt"
    changed=$?
    if ((changed != 0 && result == 0)); then result=1; fi
    printf 'exit=%s\nphase=%s\nchanged-inputs=%s\nartifacts=%s\n' "$result" "$phase" "$changed" "$work" > "$work.report.txt"
    if ((result == 0)) && $own_work; then rm -rf -- "$work" || result=$?;
    elif ((result != 0)); then printf '[csv] FAIL phase=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
run() {
    phase=$1
    local seconds=$2 result=0
    shift 2
    printf '[csv] %s\n' "$phase"
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    timeout "${seconds}s" "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\n' "$result" > "$work/$phase.exit"
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}
observe() {
    if run "$@"; then :; else failures=$((failures + 1)); fi
}
golden() {
    # 只统一行尾，正文中的独立 CR 仍参与精确比较。
    sed 's/\r$//' "$1" > "$work/expected.txt"
    diff -u "$work/expected.txt" "$2"
}
build_run() {
    if run "$1-build" 120 "$xl" build "$2" -o "$work/$1"; then
        observe "$1-run" 45 "$work/$1"
    else failures=$((failures + 1)); fi
}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run compiler-info 120 "$cc" --version
run runtime-compile 120 "$cc" "${flags[@]}" -DXRT_MODULE_THREAD -DXRT_MODULE_ATOMIC -c test/support/extlib_csv_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
mapfile -t sources < <(python3 -c 'import json; print("\n".join(json.load(open("release/extlibs/csv/xlang.package.json"))["artifact"]["cSources"]))')
for source in "${sources[@]}"; do
    object="$work/${source%.c}.o"
    run "${source%.c}-compile" 120 "$cc" "${flags[@]}" -Wmissing-prototypes -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
for name in test_csv test_csv_boundary test_csv_stream test_csv_error_cause test_csv_alloc test_csv_files test_csv_contracts test_csv_threads; do
    run "$name-compile" 120 "$cc" "${flags[@]}" "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    run "$name-run" 90 "$work/$name"
done
run interop-compile 120 "$cc" "${flags[@]}" "$pkg/test/csv_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
run python-interop 180 python3 tools/check_extlib_csv_interop.py "$work/interop" "$work/interop-data"
run closed-rollback-compile 120 "$cc" "${flags[@]}" "$pkg/test/file_rollback_closed.c" "${objects[@]}" -lm -ldl -o "$work/file_rollback_closed"
observe closed-rollback 90 "$work/file_rollback_closed" "$work"

# 原始语言正例分别执行；某项失败不掩盖后续独立边界，最终仍返回非零。
for name in central full boundaries files aot_core global_lifecycle; do
    source="$pkg/test/$name.xl"
    [[ $name != central ]] || source=test/cases/extlib_csv.xl
    [[ $name != full ]] || source=release/examples/extlibs/csv.xl
    observe "$name-jit" 90 "$xl" run "$source"
    build_run "$name-aot" "$source"
    run "$name-cgen" 90 "$xl" cgen "$source" --emit "$work/module-$name.c"
done
grep -hE '^extern .*xcsv_[a-z_]+\(' "$work"/module-*.c | sort -u > "$work/decls.txt"
grep -oE 'extern "xcsv_[a-z_]+"' "$pkg/src/csv.xl" | cut -d '"' -f 2 | sort -u > "$work/expected-symbols.txt"
grep -oE 'xcsv_[a-z_]+\(' "$work/decls.txt" | tr -d '(' | sort -u > "$work/actual-symbols.txt"
diff -u "$work/expected-symbols.txt" "$work/actual-symbols.txt"
[[ $(wc -l < "$work/decls.txt") == $(wc -l < "$work/expected-symbols.txt") ]]
printf '#include "csv_native.h"\n' > "$work/abi.c"
cat "$work/decls.txt" >> "$work/abi.c"
run generated-abi 120 "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
for name in native constructor; do
    status=0
    run "private-$name" 90 "$xl" check "$pkg/test/private_$name.xl" || status=$?
    cat "$work/private-$name.stdout" "$work/private-$name.stderr" > "$work/private-$name.txt"
    if [[ $status != 1 ]] || ! grep -Fq access "$work/private-$name.txt"; then
        failures=$((failures + 1))
    elif [[ $name == native ]]; then grep -Fq fieldNative "$work/private-$name.txt"; fi
done
for name in basic files streaming; do
    c_source="$pkg/examples/$name.c"
    xl_source="release/examples/extlibs/csv_$name.xl"
    expected="release/examples/extlibs/csv_$name.expected.txt"
    if [[ $name == streaming ]]; then
        c_source="$pkg/examples/streaming_c.c"
        xl_source="$pkg/examples/streaming.xl"
        expected="$pkg/examples/streaming.expected.txt"
    fi
    run "$name-c-compile" 120 "$cc" "${flags[@]}" "$c_source" "$work/runtime.o" -lm -ldl -o "$work/$name"
    run "$name-c-run" 90 "$work/$name"
    golden "$expected" "$work/$name-c-run.stdout"
    run "$name-c-direct" 90 "$xl" run "$c_source"
    golden "$expected" "$work/$name-c-direct.stdout"
    build_run "$name-c-aot" "$c_source"
    if [[ -f $work/$name-c-aot-run.stdout ]]; then golden "$expected" "$work/$name-c-aot-run.stdout"; fi
    observe "$name-jit" 90 "$xl" run "$xl_source"
    build_run "$name-xl-aot" "$xl_source"
    # 错误退出已经记录；有完整输出但退出失败时，仍不能当成成功。
    if [[ $(< "$work/$name-jit.exit") == 0 ]]; then golden "$expected" "$work/$name-jit.stdout"; fi
    if [[ -f $work/$name-xl-aot-run.exit ]] && [[ $(< "$work/$name-xl-aot-run.exit") == 0 ]]; then
        golden "$expected" "$work/$name-xl-aot-run.stdout"
    fi
done
if [[ -n ${CSV_DECLARATIONS:-} ]]; then run supplied-abi 120 "$cc" "${flags[@]}" -fsyntax-only "$CSV_DECLARATIONS"; fi
phase=language-summary
if ((failures)); then printf 'CSV language boundary failures: %s\n' "$failures" >&2; exit 1; fi
printf 'CSV Linux C ASan/UBSan and XLang parity gate: PASS\n'
