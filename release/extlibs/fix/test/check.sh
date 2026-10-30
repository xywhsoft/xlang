#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-fix-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}; sequence=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
input_paths() {
    find release/extlibs/fix release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so test/cases/extlib_fix.xl release/examples/extlibs/fix.xl
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
    else printf '[fix] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[fix] report: %s.report.txt\n' "$work"
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
run 10 "$python" -c 'import sys; assert sys.version_info >= (3,10); print(sys.version)'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/fix -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
run 10 "$python" -c 'import json,pathlib,re; p=pathlib.Path("release/extlibs/fix"); a=json.loads((p/"xlang.package.json").read_text())["artifact"]["xrtModules"]; b=re.findall(r"^#define (XRT_MODULE_[A-Z_]+)\s*$",(p/"examples/fix_c.c").read_text(),re.M); assert len(a)==3 and sorted(a)==sorted(b)'
compile -x c -fsyntax-only release/extlibs/fix/fix_native.h
compile -fsyntax-only release/extlibs/fix/fix_native.c
compile -DXRT_IMPLEMENTATION release/extlibs/fix/examples/fix_c.c -no-pie -lm -ldl -o "$work/minimal"
run 120 "$work/minimal" > "$work/basic.txt"
run 30 "$python" release/extlibs/fix/test/check_frame.py "$(cat "$work/basic.txt")"
compile -c release/extlibs/fix/test/xrt_runtime.c -o "$work/runtime.o"
compile release/extlibs/fix/test/test_fix.c release/extlibs/fix/fix_native.c "$work/runtime.o" -no-pie -lm -ldl -o "$work/legacy"
run 120 "$work/legacy"
source=release/extlibs/fix/test/tag_grammar_c.c
compile "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/grammar"
run 120 "$work/grammar" > "$work/grammar.txt"
printf 'fix tag grammar: 374 cases, 4 OOM paths, 0 failures\n' > "$work/grammar.expected"
diff -u "$work/grammar.expected" "$work/grammar.txt"
run 90 "$work/grammar" --records > "$work/gcc.tsv"
run 60 "$python" release/extlibs/fix/test/check_tag_records.py "$work/gcc.tsv"
run 120 ./release/xl run "$source" > "$work/direct.txt"; diff -u "$work/grammar.expected" "$work/direct.txt"
run 90 ./release/xl run "$source" -- --records > "$work/tcc.tsv"; cmp "$work/gcc.tsv" "$work/tcc.tsv"
run 60 "$python" release/extlibs/fix/test/check_tag_records.py "$work/tcc.tsv"
run 120 ./release/xl run release/extlibs/fix/examples/fix_c.c > "$work/basic.direct.txt"; diff -u "$work/basic.txt" "$work/basic.direct.txt"
c_example=release/extlibs/fix/examples/tag_gate_c.c
compile "$c_example" "$work/runtime.o" -no-pie -lm -ldl -o "$work/tag-example"
sed 's/\r$//' release/extlibs/fix/examples/tag_gate.expected.txt > "$work/tag.expected"
run 90 "$work/tag-example" > "$work/tag.c.txt"; diff -u "$work/tag.expected" "$work/tag.c.txt"
run 90 ./release/xl run "$c_example" > "$work/tag.direct.txt"; diff -u "$work/tag.expected" "$work/tag.direct.txt"
language=release/extlibs/fix/examples/tag_gate.xl; boundaries=release/extlibs/fix/test/tag_grammar.xl
run 90 ./release/xl run "$language" > "$work/tag.xl.txt"; diff -u "$work/tag.expected" "$work/tag.xl.txt"
run 90 ./release/xl run "$boundaries"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -q 'xl6_xrt_take_cstr(' "$work/probe.c"
grep -E '^extern .*xfix_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xfix_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xfix_[a-z_]+)".*/\1/p' release/extlibs/fix/src/fix.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 8; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "fix_native.c"\n' > "$work/abi.c"; cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
index=0
for input in "$source" "$boundaries" "$language" "$c_example" release/extlibs/fix/examples/fix_c.c; do
    index=$((index+1)); binary="$work/aot-$index"
    run 120 ./release/xl build "$input" -o "$binary"; run 90 "$binary" > "$work/aot-$index.txt"
    case "$input" in
        */tag_grammar_c.c) diff -u "$work/grammar.expected" "$work/aot-$index.txt"; run 90 "$binary" --records > "$work/aot.tsv"; cmp "$work/gcc.tsv" "$work/aot.tsv"; run 60 "$python" release/extlibs/fix/test/check_tag_records.py "$work/aot.tsv" ;;
        */tag_gate.xl|*/tag_gate_c.c) diff -u "$work/tag.expected" "$work/aot-$index.txt" ;;
        */fix_c.c) diff -u "$work/basic.txt" "$work/aot-$index.txt" ;;
    esac
done
# 原 build(var fields) 正例仍是必过项，不能拿收到的报文解析成功代替。
for input in test/cases/extlib_fix.xl release/extlibs/fix/test/boundaries.xl release/extlibs/fix/test/limits.xl \
    release/extlibs/fix/test/allocations.xl release/extlibs/fix/test/aot_core.xl release/examples/extlibs/fix.xl; do
    run 120 ./release/xl run "$input" > "$work/complete.txt"
    if [[ $input == release/examples/extlibs/fix.xl ]]; then diff -u "$work/basic.txt" "$work/complete.txt"; fi
    run 120 ./release/xl build "$input" -o "$work/complete"; run 90 "$work/complete"
done
printf 'FIX grammar, sanitizer, independent frames and complete bilingual JIT/AOT: PASS\n'
