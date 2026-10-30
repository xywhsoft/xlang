#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
own_work=true
if [[ -n ${GRAPH_WORK_DIR:-} ]]; then work=$GRAPH_WORK_DIR; mkdir -p -- "$work"; own_work=false
else work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-graph-XXXXXX"); fi
pkg=release/extlibs/graph
cc=${CC:-gcc}; python=${PYTHON:-python3}; sequence=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
input_paths() {
    find "$pkg" release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_graph_runtime.c test/support/extlib_runtime_host.c tools/check_extlib_graph.sh test/cases/extlib_graph.xl \
        release/examples/extlibs/graph.xl release/examples/extlibs/graph_basic.xl release/examples/extlibs/graph_basic.expected.txt
    if [[ -n $cc_path ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n $python_path ]]; then printf '%s\0' "$python_path"; fi
    if [[ -n ${GRAPH_DECLARATIONS:-} ]]; then printf '%s\0' "$GRAPH_DECLARATIONS"; fi
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
    if ((result==0)) && $own_work; then rm -rf -- "$work" || result=$?
    elif ((result!=0)); then printf '[graph] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[graph] report: %s.report.txt\n' "$work"
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
export PYTHONDONTWRITEBYTECODE=1 XLANG_PACKAGE_PATH="$root/release/extlibs"
run 10 "$python" -c 'import json,sys; p=json.load(open("release/extlibs/graph/xlang.package.json")); assert sorted(p["artifact"]["xrtModules"])==["XRT_MODULE_ARRAY","XRT_MODULE_MAP","XRT_MODULE_VALUE_CONTAINER"]; print(sys.version)'
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -t modules < <("$python" -c 'import json; print("\n".join("-D"+x for x in json.load(open("release/extlibs/graph/xlang.package.json"))["artifact"]["xrtModules"]))')
compile "${modules[@]}" -DXRT_IMPLEMENTATION "$pkg/test/manifest_probe.c" -no-pie -lm -ldl -o "$work/minimal"
run 120 "$work/minimal"
compile -c test/support/extlib_graph_runtime.c -o "$work/runtime.o"
mapfile -t sources < <("$python" -c 'import json; print("\n".join(json.load(open("release/extlibs/graph/xlang.package.json"))["artifact"]["cSources"]))')
objects=()
for source in "${sources[@]}"; do
    object="$work/native-${#objects[@]}.o"
    compile -c "$pkg/$source" -o "$object"; objects+=("$object")
done
for name in test_graph test_graph_alloc test_graph_threads; do
    compile -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name"
done
weighted="$pkg/test/weighted_records_c.c"
compile -DNDEBUG "$weighted" "$work/runtime.o" -no-pie -lm -ldl -o "$work/weighted"
run 120 "$work/weighted" > "$work/weighted.txt"
printf 'graph weighted records: 3072 cases, PASS\n' > "$work/weighted.expected"
diff -u "$work/weighted.expected" "$work/weighted.txt"
run 120 "$work/weighted" --records > "$work/weighted.tsv"
run 60 "$python" "$pkg/test/check_weighted_records.py" "$work/weighted.tsv"
run 120 ./release/xl run "$weighted" -- --records > "$work/weighted-direct.tsv"
cmp "$work/weighted.tsv" "$work/weighted-direct.tsv"
example="$pkg/examples/basic.c"
compile -DNDEBUG "$example" "$work/runtime.o" -no-pie -lm -ldl -o "$work/basic"
run 90 "$work/basic" > "$work/basic.log"
sed 's/\r$//' release/examples/extlibs/graph_basic.expected.txt > "$work/basic.expected"
diff -u "$work/basic.expected" "$work/basic.log"
run 90 ./release/xl run "$example" > "$work/basic-direct.log"; diff -u "$work/basic.expected" "$work/basic-direct.log"
language="$pkg/examples/from_json.xl"
run 90 ./release/xl run "$language" > "$work/from-json.log"; diff -u "$work/basic.expected" "$work/from-json.log"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -E '^extern .*xg_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/declarations.inc"
grep -oE 'xg_[a-z_]+[(]' "$work/declarations.inc" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xg_[a-z_]+)".*/\1/p' "$pkg/src/graph.xl" | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/declarations.inc")" -eq 9; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "graph.h"\n' > "$work/abi.c"; cat "$work/declarations.inc" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
if [[ -n ${GRAPH_DECLARATIONS:-} ]]; then compile -include "$pkg/include/graph.h" -x c -fsyntax-only "$GRAPH_DECLARATIONS"; fi
index=0
for source in "$language" "$weighted" "$example"; do
    index=$((index+1)); binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"; run 120 "$binary" > "$work/aot-$index.txt"
    if [[ $source == "$weighted" ]]; then
        diff -u "$work/weighted.expected" "$work/aot-$index.txt"
        run 120 "$binary" --records > "$work/weighted-aot.tsv"; cmp "$work/weighted.tsv" "$work/weighted-aot.tsv"
        run 60 "$python" "$pkg/test/check_weighted_records.py" "$work/weighted-aot.tsv"
    else diff -u "$work/basic.expected" "$work/aot-$index.txt"; fi
done
cat "$work/basic.expected" "$work/basic.expected" "$work/basic.expected" > "$work/context.expected"
for library in xl xlc; do
    run 120 "$cc" -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include test/support/extlib_runtime_host.c \
        "$root/release/lib$library.so" "-Wl,-rpath,$root/release" -pthread -lm -ldl -o "$work/$library-host"
    run 120 "$work/$library-host" "$root" "$root/$language" > "$work/$library-context.log"
    diff -u "$work/context.expected" "$work/$library-context.log"
done
# 保留原语言字面量输入及双语门禁，不能用 JSON 输入替代。
for source in test/cases/extlib_graph.xl "$pkg/test/boundaries.xl" "$pkg/test/allocations.xl" \
    release/examples/extlibs/graph.xl release/examples/extlibs/graph_basic.xl; do
    run 120 ./release/xl run "$source" > "$work/complete.txt"
    if [[ $source == */graph_basic.xl ]]; then diff -u "$work/basic.expected" "$work/complete.txt"; fi
    run 120 ./release/xl build "$source" -o "$work/complete"; run 90 "$work/complete"
done
printf 'graph numeric oracle, C/JSON-input golden, ABI and complete JIT/AOT: PASS\n'
