#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-json-patch-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}; sequence=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
input_paths() {
    find release/extlibs/json.patch release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so test/cases/extlib_json_patch.xl release/examples/extlibs/json_patch.xl
    if [[ -n $cc_path ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n $python_path ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    result=$?; trap - EXIT; set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\nphases=%s\n' "$result" "$sequence" > "$work.report.txt"
    if [[ -f $work/commands.log ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[json.patch] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[json.patch] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 result; shift; sequence=$((sequence+1))
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"; printf ' %q' "$@" >> "$work/commands.log"; printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"; cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"; cat "$work/$sequence.err" >&2
    return "$result"
}
run 10 "$cc" --version > "$work/compiler.txt"
run 10 "$python" -c 'import json,sys; p=json.load(open("release/extlibs/json.patch/xlang.package.json")); assert sorted(p["artifact"]["xrtModules"]) == ["XRT_MODULE_UNICODE","XRT_MODULE_VALUE_GRAPH"]; print(sys.version)'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/json.patch/include -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
native=release/extlibs/json.patch/json_patch_native.c
compile -x c -fsyntax-only release/extlibs/json.patch/include/json_patch.h
compile -DXRT_MODULE_VALUE_GRAPH -DXRT_MODULE_UNICODE -fsyntax-only "$native"
compile -DXRT_MODULE_VALUE_GRAPH -DXRT_MODULE_UNICODE -DXRT_IMPLEMENTATION release/extlibs/json.patch/test/manifest_probe.c -no-pie -lm -ldl -o "$work/minimal"
run 120 "$work/minimal"
compile -c release/extlibs/json.patch/test/xrt_runtime.c -o "$work/runtime.o"
compile "$native" release/extlibs/json.patch/test/test_boundaries.c "$work/runtime.o" -no-pie -lm -ldl -o "$work/boundaries"
run 120 "$work/boundaries"
compile "$native" release/extlibs/json.patch/test/oracle_cases.c "$work/runtime.o" -no-pie -lm -ldl -o "$work/oracle"
source=release/extlibs/json.patch/test/move_ownership_c.c
compile "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/move"
run 120 "$work/move" > "$work/move.txt"
printf 'json.patch move: 32 cases, 16 OOM paths, 0 semantic failures, 0 clone-budget failures\n' > "$work/move.expected"
diff -u "$work/move.expected" "$work/move.txt"
run 120 "$work/move" --costs > "$work/gcc-costs.tsv"
test "$(wc -l < "$work/gcc-costs.tsv")" -eq 12
run 120 ./release/xl run "$source" > "$work/direct.txt"; diff -u "$work/move.expected" "$work/direct.txt"
run 120 ./release/xl run "$source" -- --costs > "$work/tcc-costs.tsv"; cmp "$work/gcc-costs.tsv" "$work/tcc-costs.tsv"
for name in basic workflow relocate; do
    input="release/extlibs/json.patch/examples/${name}_c.c"
    compile "$input" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$input" > "$work/$name.direct.txt"; diff -u "$work/$name.c.txt" "$work/$name.direct.txt"
done
language=release/extlibs/json.patch/examples/relocate.xl
sed 's/\r$//' release/extlibs/json.patch/examples/relocate.expected.txt > "$work/relocate.expected"
diff -u "$work/relocate.expected" "$work/relocate.c.txt"
run 90 ./release/xl run "$language" > "$work/relocate.xl.txt"; diff -u "$work/relocate.expected" "$work/relocate.xl.txt"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -E '^extern .*xjsonpatch_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xjsonpatch_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xjsonpatch_[a-z_]+)".*/\1/p' release/extlibs/json.patch/src/json_patch.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 8; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "json_patch.h"\n' > "$work/abi.c"; cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
index=0
for input in "$source" "$language" release/extlibs/json.patch/examples/basic_c.c release/extlibs/json.patch/examples/workflow_c.c release/extlibs/json.patch/examples/relocate_c.c; do
    index=$((index+1)); binary="$work/aot-$index"
    run 120 ./release/xl build "$input" -o "$binary"; run 90 "$binary" > "$work/aot-$index.txt"
    case "$input" in
        */move_ownership_c.c) diff -u "$work/move.expected" "$work/aot-$index.txt"; run 120 "$binary" --costs > "$work/aot-costs.tsv"; cmp "$work/gcc-costs.tsv" "$work/aot-costs.tsv" ;;
        */relocate.xl|*/relocate_c.c) diff -u "$work/relocate.expected" "$work/aot-$index.txt" ;;
        */basic_c.c) diff -u "$work/basic.c.txt" "$work/aot-$index.txt" ;;
        */workflow_c.c) diff -u "$work/workflow.c.txt" "$work/aot-$index.txt" ;;
    esac
done
# 从标准 JSON 解析出的原生值可操作，不等于语言字面量投影已通过。
for input in test/cases/extlib_json_patch.xl release/extlibs/json.patch/test/boundaries.xl release/extlibs/json.patch/test/allocations.xl \
    release/extlibs/json.patch/test/aot_core.xl release/extlibs/json.patch/examples/basic.xl release/extlibs/json.patch/examples/workflow.xl release/examples/extlibs/json_patch.xl; do
    run 120 ./release/xl run "$input" > "$work/complete.txt"
    if [[ $input == */examples/basic.xl ]]; then diff -u "$work/basic.c.txt" "$work/complete.txt"; fi
    if [[ $input == */examples/workflow.xl ]]; then diff -u "$work/workflow.c.txt" "$work/complete.txt"; fi
    run 120 ./release/xl build "$input" -o "$work/complete"; run 90 "$work/complete"
done
printf 'JSON Patch move ownership, sanitizer and complete bilingual JIT/AOT: PASS\n'
