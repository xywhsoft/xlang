#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-shellwords-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/shellwords release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_shellwords.xl release/examples/extlibs/shellwords.xl
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
    else printf '[shellwords] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[shellwords] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/shellwords
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/shellwords/include/xrt_shellwords.h
compile -DXRT_MODULE_BUFFER -DXRT_MODULE_VALUE_CONTAINER -fsyntax-only release/extlibs/shellwords/shellwords_native.c
compile -c release/extlibs/shellwords/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/shellwords/shellwords_native.c -o "$work/native.o"
for name in test_shellwords quote_runs_c; do
    objects=("$work/runtime.o")
    if [[ $name == test_shellwords ]]; then objects+=("$work/native.o"); fi
    compile "release/extlibs/shellwords/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'shellwords quote runs: 126 vectors, 8 OOM paths, PASS\n' > "$work/quote.expected"
diff -u "$work/quote.expected" "$work/quote_runs_c.txt"
run 90 "$work/quote_runs_c" --records > "$work/gcc.tsv"
run 60 "$python" release/extlibs/shellwords/test/check_quote_records.py "$work/gcc.tsv" --shell
source=release/extlibs/shellwords/test/quote_runs_c.c
run 120 ./release/xl run "$source" > "$work/quote.direct"
diff -u "$work/quote.expected" "$work/quote.direct"
run 90 ./release/xl run "$source" -- --records > "$work/tcc.tsv"
cmp "$work/gcc.tsv" "$work/tcc.tsv"
run 60 "$python" release/extlibs/shellwords/test/check_quote_records.py "$work/tcc.tsv" --shell
for name in basic command_plan; do
    c=release/extlibs/shellwords/examples/basic.c; language=release/examples/extlibs/shellwords.xl
    if [[ $name != basic ]]; then
        c=release/extlibs/shellwords/examples/command_plan_c.c
        language=release/extlibs/shellwords/examples/command_plan.xl
    fi
    sed 's/\r$//' "release/extlibs/shellwords/examples/$name.expected.txt" > "$work/$name.expected"
    compile "$c" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$c" > "$work/$name.direct.txt"
    run 90 ./release/xl run "$language" > "$work/$name.xl.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.xl.txt"
done
texts=release/extlibs/shellwords/test/text_paths.xl
run 120 ./release/xl run "$texts"
run 90 ./release/xl cgen release/extlibs/shellwords/examples/command_plan.xl --emit "$work/probe.c"
grep -Eq 'xl6_xrt_take_cstr\(' "$work/probe.c"
grep -E '^extern .*(xsw_[a-z_]+|xrtShellwordsSplit)[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE '(xsw_[a-z_]+|xrtShellwordsSplit)[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "([A-Za-z0-9_]+)".*/\1/p' release/extlibs/shellwords/src/shellwords.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 4
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "shellwords_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
sources=(release/extlibs/shellwords/test/quote_runs_c.c "$texts" release/extlibs/shellwords/examples/basic.c
         release/examples/extlibs/shellwords.xl release/extlibs/shellwords/examples/command_plan_c.c
         release/extlibs/shellwords/examples/command_plan.xl)
index=0
for source in "${sources[@]}"; do
    index=$((index+1))
    binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"
    run 90 "$binary" > "$work/aot-$index.txt"
    case "$source" in
        */quote_runs_c.c)
            diff -u "$work/quote.expected" "$work/aot-$index.txt"
            run 90 "$binary" --records > "$work/aot.tsv"
            cmp "$work/gcc.tsv" "$work/aot.tsv"
            run 60 "$python" release/extlibs/shellwords/test/check_quote_records.py "$work/aot.tsv" --shell ;;
        */basic.c|release/examples/extlibs/shellwords.xl) diff -u "$work/basic.expected" "$work/aot-$index.txt" ;;
        */command_plan_c.c|*/command_plan.xl) diff -u "$work/command_plan.expected" "$work/aot-$index.txt" ;;
    esac
done
# 真实语言数组依旧必须通过，不以 split 的原生数组结果绕过 M1。
for source in test/cases/extlib_shellwords.xl release/extlibs/shellwords/test/boundaries.xl release/extlibs/shellwords/test/allocations.xl; do
    run 120 ./release/xl run "$source"
    run 120 ./release/xl build "$source" -o "$work/complete"
    run 90 "$work/complete"
done
printf 'shellwords Linux sanitizer, independent shell, actual ABI and bilingual JIT/AOT: PASS\n'
