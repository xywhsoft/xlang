#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-srt-XXXXXX")
pkg=release/extlibs/srt
cc=${CC:-gcc}; python=${PYTHON:-python3}; phases=0
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxlc.so test/cases/extlib_srt.xl \
        release/examples/extlibs/srt.xl release/examples/extlibs/srt_pair.xl release/examples/extlibs/srt_c.c
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
    else printf '[srt] FAIL exit=%s artifacts=%s\n' "$status" "$work" >&2; fi
    printf '[srt] report: %s.report.txt\n' "$work"
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
run 10 "$python" -c 'import json,sys; assert sorted(json.load(open("release/extlibs/srt/xlang.package.json"))["artifact"]["xrtModules"])==["XRT_MODULE_BUFFER","XRT_MODULE_UNICODE","XRT_MODULE_VALUE_CONTAINER"]; print(sys.version)'
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -I"$pkg/include"
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
compile -DXRT_IMPLEMENTATION "$pkg/examples/normalize_c.c" -no-pie -lm -ldl -o "$work/minimal"
sed 's/\r$//' "$pkg/examples/normalize.expected.txt" > "$work/normalize.expected"
run 60 "$work/minimal" > "$work/minimal.txt"; diff -u "$work/normalize.expected" "$work/minimal.txt"
compile -c "$pkg/test/xrt_runtime.c" -o "$work/runtime.o"
compile -c "$pkg/srt_native.c" -o "$work/native.o"
compile -fsyntax-only "$pkg/test/abi_contract.c"
compile -DNDEBUG "$pkg/test/test_srt.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/legacy"
run 120 "$work/legacy" > "$work/legacy.txt"; printf 'srt C boundaries: PASS\n' | diff -u - "$work/legacy.txt"
budget="$pkg/test/owned_results_c.c"
compile -DNDEBUG "$budget" "$work/runtime.o" -no-pie -lm -ldl -o "$work/budgets"
run 120 "$work/budgets" > "$work/budgets.txt"; printf 'srt C owned results: 22 budgets, PASS\n' | diff -u - "$work/budgets.txt"
printf '0\t1\n1\t1\n2\t1\n3\t3\n4\t2\n5\t8\n' > "$work/costs.expected"
run 120 "$work/budgets" --costs > "$work/gcc-costs.txt"; diff -u "$work/costs.expected" "$work/gcc-costs.txt"
run 120 ./release/xl run "$budget" -- --costs > "$work/direct-costs.txt"; cmp "$work/gcc-costs.txt" "$work/direct-costs.txt"
run 120 ./release/xl cgen "$pkg/test/owned_probe.xl" --emit "$work/owned.c"
run 60 "$python" -c 'import re,sys; s=open(sys.argv[1]).read(); bodies=re.findall(r"(?ms)^static xlir_result_string xlir_fn_\d+\([^\n]*\)\r?\n\{.*?^\}",s); assert len(bodies)==3; assert all(len(re.findall(r"\b\w+\.Data\s*=\s*xsrt_"+name+r"\s*\(",s))==1 for name in ["build","build_vtt","format_time"]); assert all(not re.search(r"xlir_string_copy\(|xl6_xrt_take_cstr\(|xl6CStringCopyV1",b) for b in bodies); assert "xl6_xrt_take_cstr(" not in s' "$work/owned.c"
grep -E '^extern .*\bxsrt_[a-z_]+[(]' "$work/owned.c" | sort -u > "$work/declarations.inc"
grep -oE 'xsrt_[a-z_]+[(]' "$work/declarations.inc" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "(xsrt_[a-z_]+)".*/\1/p' "$pkg/src/srt.xl" | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/declarations.inc")" -eq 6; diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "srt_native.h"\n' > "$work/abi.c"; cat "$work/declarations.inc" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
legacy=release/examples/extlibs/srt_c.c
compile -DNDEBUG "$legacy" "$work/runtime.o" -no-pie -lm -ldl -o "$work/legacy-example"
run 120 "$work/legacy-example" > "$work/legacy.expected"
run 120 ./release/xl run "$legacy" > "$work/legacy-direct.txt"; diff -u "$work/legacy.expected" "$work/legacy-direct.txt"
run 120 ./release/xl run release/examples/extlibs/srt_pair.xl > "$work/legacy-xl.txt"; diff -u "$work/legacy.expected" "$work/legacy-xl.txt"
example="$pkg/examples/normalize_c.c"; language="$pkg/examples/normalize.xl"; owned="$pkg/test/owned_results.xl"; probe="$pkg/test/owned_probe.xl"; core="$pkg/test/core_aot.xl"
compile -DNDEBUG "$example" "$work/runtime.o" -no-pie -lm -ldl -o "$work/normalize"
run 120 "$work/normalize" > "$work/normalize.txt"; diff -u "$work/normalize.expected" "$work/normalize.txt"
run 120 ./release/xl run "$example" > "$work/normalize-direct.txt"; diff -u "$work/normalize.expected" "$work/normalize-direct.txt"
run 120 ./release/xl run "$owned" > "$work/owned.txt"; printf 'srt owned results: 22 budgets, PASS\n' | diff -u - "$work/owned.txt"
run 120 ./release/xl run "$probe" > "$work/probe.txt"; test ! -s "$work/probe.txt"
run 120 ./release/xl run "$language" > "$work/normalize-xl.txt"; diff -u "$work/normalize.expected" "$work/normalize-xl.txt"
for file in "$budget" "$owned" "$probe" "$language" "$example" "$legacy" release/examples/extlibs/srt_pair.xl; do
    run 120 ./release/xl build "$file" -o "$work/aot"; run 120 "$work/aot" > "$work/aot.txt"
    case "$file" in
        "$budget") diff -u "$work/budgets.txt" "$work/aot.txt"; run 120 "$work/aot" --costs > "$work/aot-costs.txt"; cmp "$work/gcc-costs.txt" "$work/aot-costs.txt" ;;
        "$owned") diff -u "$work/owned.txt" "$work/aot.txt" ;;
        "$language"|"$example") diff -u "$work/normalize.expected" "$work/aot.txt" ;;
        "$legacy"|release/examples/extlibs/srt_pair.xl) diff -u "$work/legacy.expected" "$work/aot.txt" ;;
        "$probe") test ! -s "$work/aot.txt" ;;
    esac
done
# 保留语言数组构建与原分配用例，不因原生解析结果可用而改成预期失败。
for file in test/cases/extlib_srt.xl "$pkg/test/boundaries.xl" "$pkg/test/allocations.xl" "$core" release/examples/extlibs/srt.xl; do
    run 120 ./release/xl run "$file"
    run 120 ./release/xl build "$file" -o "$work/complete"; run 120 "$work/complete"
done
printf 'srt complete C/XLang gate: PASS\n'
