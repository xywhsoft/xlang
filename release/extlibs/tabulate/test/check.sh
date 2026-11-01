#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-tabulate-XXXXXX")
finish() {
    status=$?
    case "$work" in "${TMPDIR:-/tmp}"/xlang-tabulate-*) ;;
        *) printf 'Unsafe tabulate cleanup path: %s\n' "$work" >&2; exit 125;; esac
    if [[ $status == 0 ]]; then rm -rf -- "$work"
    else printf 'tabulate failure artifacts: %s\n' "$work" >&2; fi
}
trap finish EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/tabulate/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
    release/extlibs/tabulate/test/test_render_budget.c -lm -ldl -o "$work/render_budget"
mkdir "$work/corpus"
timeout 90s "$work/render_budget" "$work/corpus"
timeout 30s python3 -B release/extlibs/tabulate/test/check_render_corpus.py "$work/corpus"

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/tabulate/test/abi_contract.c
"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/tabulate/include -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_UNICODE release/extlibs/tabulate/tabulate_native.c \
    release/extlibs/tabulate/tabulate_render.c

"$cc" "${flags[@]}" -c release/extlibs/tabulate/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/tabulate/tabulate_native.c \
    -o "$work/tabulate_native.o"
"$cc" "${flags[@]}" -c release/extlibs/tabulate/tabulate_render.c \
    -o "$work/tabulate_render.o"
for test in test_tabulate test_threads; do
    "$cc" "${flags[@]}" "release/extlibs/tabulate/test/$test.c" \
        "$work/tabulate_native.o" "$work/tabulate_render.o" \
        "$work/xrt_runtime.o" -lm -ldl -o "$work/$test"
    timeout 90s "$work/$test"
done
"$cc" "${flags[@]}" release/extlibs/tabulate/examples/tabulate_c.c \
    "$work/tabulate_native.o" "$work/tabulate_render.o" \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/tabulate_example"
timeout 30s "$work/tabulate_example" > "$work/c_example.txt"
diff -u release/extlibs/tabulate/test/tabulate.expected.txt "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/tabulate/examples/tabulate_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_tabulate.xl
timeout 90s ./release/xl run release/extlibs/tabulate/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/tabulate/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/tabulate/test/render_budget.xl
timeout 90s ./release/xl run release/extlibs/tabulate/test/return_budget.xl
timeout 90s ./release/xl run release/extlibs/tabulate/test/test_render_budget.c
timeout 90s ./release/xl cgen test/cases/extlib_tabulate.xl --emit "$work/owned.c"
timeout 30s python3 -B release/extlibs/tabulate/test/check_owned_codegen.py "$work/owned.c"
if timeout 90s ./release/xl check \
    release/extlibs/tabulate/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'tabulate: raw native cell append must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'appendCellNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'tabulate: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/tabulate.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
printf 'rows: 256\noversized render: rejected\ntable preserved: true\nrecovered render: true\n' > "$work/bounded.expected"
"$cc" "${flags[@]}" release/extlibs/tabulate/examples/bounded_report_c.c \
    "$work/tabulate_native.o" "$work/tabulate_render.o" \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/bounded_report"
timeout 30s "$work/bounded_report" > "$work/bounded-gcc.txt"
diff -u "$work/bounded.expected" "$work/bounded-gcc.txt"
for source in bounded_report_c.c bounded_report.xl; do
    timeout 90s ./release/xl run "release/extlibs/tabulate/examples/$source" > "$work/$source.txt"
    diff -u "$work/bounded.expected" "$work/$source.txt"
done
if [[ "${TABULATE_SKIP_AOT:-0}" == "1" ]]; then
    printf 'tabulate Linux C ASan/UBSan and XLang focused gate: PASS (AOT skipped)\n'
    exit 0
fi
for source in \
    test/cases/extlib_tabulate.xl \
    release/extlibs/tabulate/test/render_budget.xl \
    release/extlibs/tabulate/test/return_budget.xl \
    release/extlibs/tabulate/test/test_render_budget.c; do
    name=$(basename "$source")
    timeout 90s ./release/xl build "$source" -o "$work/$name"
    timeout 30s "$work/$name"
done
timeout 90s ./release/xl build release/examples/extlibs/tabulate.xl \
    -o "$work/tabulate_xlang"
timeout 30s "$work/tabulate_xlang" > "$work/xlang_aot.txt"
diff -u release/extlibs/tabulate/test/tabulate.expected.txt "$work/xlang_aot.txt"
timeout 90s ./release/xl build release/extlibs/tabulate/examples/tabulate_c.c \
    -o "$work/tabulate_c"
timeout 30s "$work/tabulate_c" > "$work/c_aot.txt"
diff -u release/extlibs/tabulate/test/tabulate.expected.txt "$work/c_aot.txt"
for source in bounded_report_c.c bounded_report.xl; do
    timeout 90s ./release/xl build "release/extlibs/tabulate/examples/$source" -o "$work/$source.aot"
    timeout 30s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/bounded.expected" "$work/$source.aot.txt"
done
# 保留两条真实全局正例的红灯，同时收集退出状态，不吞掉失败。
failed=0
for source in boundaries allocations; do
    timeout 90s ./release/xl build "release/extlibs/tabulate/test/$source.xl" -o "$work/$source"
    if timeout 90s "$work/$source" > "$work/$source.stdout" 2> "$work/$source.stderr"; then
        printf 'tabulate global AOT %s: PASS\n' "$source"
    else
        code=$?
        printf 'tabulate global AOT %s: FAIL exit=%s\n' "$source" "$code" >&2
        cat "$work/$source.stdout" "$work/$source.stderr" >&2
        failed=$((failed + 1))
    fi
done
if [[ $failed != 0 ]]; then exit 1; fi
printf 'tabulate Linux C ASan/UBSan and bilingual AOT gate: PASS\n'
