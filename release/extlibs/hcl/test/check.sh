#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-hcl-XXXXXX")
phase=setup
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '%s\n' "$phase" > "$work/failed-stage.txt"
        printf '%s\n' "$result" > "$work/exit-code.txt"
        printf '[hcl] FAIL stage=%s exit=%s artifacts=%s\n' "$phase" "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT

# 保存子进程的原始状态和两条输出流；失败时不删除现场。
run() {
    phase=$1
    shift
    local result=0
    printf '[hcl] %s\n' "$phase" >&2
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || result=$?
    printf '%s\t%s\n' "$phase" "$result" >> "$work/phases.tsv"
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    return "$result"
}

cc=${CC:-gcc}
pkg=release/extlibs/hcl
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find "$pkg" -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl \
          release/libxl.so release/libxlc.so release/include/xrt_decl.h \
          release/include/xruntime_decl.h release/include/xl_extlib_abi.h test/cases/extlib_hcl.xl \
          release/examples/extlibs/hcl.xl release/examples/extlibs/hcl_c.c
    } | sort -zu
)
run fingerprint sha256sum "${inputs[@]}" > /dev/null
run compiler-info "$cc" --version > "$work/compiler.txt"
run abi "$cc" "${flags[@]}" -fsyntax-only release/extlibs/hcl/test/abi_contract.c
run c-test-compile "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/extlibs/hcl/test/test_hcl.c -o "$work/test_hcl.o"
run c-native-compile "$cc" "${flags[@]}" -c release/extlibs/hcl/hcl_native.c \
    -o "$work/hcl_native.o"
run c-test-link "$cc" "${flags[@]}" -no-pie "$work/test_hcl.o" "$work/hcl_native.o" \
    -lm -ldl -o "$work/test_hcl"
run c-test-run timeout 90s "$work/test_hcl"

# 单字符串解码/复合表达式原文，以及两条返回路径的逐分配点故障。
run c-expression-compile "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -no-pie \
    "$pkg/test/test_expression_strings.c" -lm -ldl -o "$work/hcl_expressions"
run c-expression-run timeout 90s "$work/hcl_expressions"
run c-query-compile "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -no-pie \
    "$pkg/examples/query_comments_c.c" -lm -ldl -o "$work/hcl_query"
printf 'port: 8080\n[\n  "api.example", // note: "retired\n  "admin.example"\n]\nHCL query comments: OK\n' > "$work/query.golden"
run c-query-run timeout 90s "$work/hcl_query" > "$work/query.c.txt"
run parity-query-gcc diff -u "$work/query.golden" "$work/query.c.txt"

# 复用已有四线程用例，只带入查询器和线程实际需要的 XRT 模块。
run c-threads-compile "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION \
    -DXRT_MODULE_BUFFER -DXRT_MODULE_CHARSET -DXRT_MODULE_NUMBER_FLOAT \
    -DXRT_MODULE_NUMBER_INTEGER -c "$pkg/test/test_hcl_threads.c" \
    -o "$work/hcl_threads.o"
run c-threads-link "$cc" "${flags[@]}" -no-pie "$work/hcl_threads.o" \
    "$work/hcl_native.o" -lm -ldl -o "$work/hcl_threads"
run c-threads-run timeout 90s "$work/hcl_threads"

run c-example-compile "$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/examples/extlibs/hcl_c.c -o "$work/hcl_example.o"
run c-example-link "$cc" "${flags[@]}" -no-pie "$work/hcl_example.o" \
    -lm -ldl -o "$work/hcl_example"
run c-example-run timeout 90s "$work/hcl_example" > "$work/hcl_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'hcl Linux gate: release/xl is required\n' >&2
    exit 1
fi
run jit-central timeout 90s "$xl" run test/cases/extlib_hcl.xl
run jit-boundaries timeout 90s "$xl" run release/extlibs/hcl/test/boundaries.xl
run jit-allocations timeout 90s "$xl" run release/extlibs/hcl/test/allocations.xl
run jit-expressions timeout 90s "$xl" run "$pkg/test/expression_strings.xl"
run jit-c-expressions timeout 90s "$xl" run "$pkg/test/test_expression_strings.c"
run generated-c timeout 90s "$xl" cgen "$pkg/test/boundaries.xl" --emit "$work/generated.c"
grep -E '^extern .*\bxhcl_[a-z_]+\([^;]*\);$' "$work/generated.c" > "$work/generated.prototypes"
if [[ $(wc -l < "$work/generated.prototypes") -ne 12 ]]; then
    printf 'hcl cgen must declare all 12 C functions\n' >&2
    exit 1
fi
{ printf '#include "hcl_native.h"\n'; cat "$work/generated.prototypes"; } > "$work/generated_abi.c"
run generated-abi "$cc" "${flags[@]}" -I"$pkg/include" -fsyntax-only "$work/generated_abi.c"
for source in query_comments.xl query_comments_c.c; do
    run "jit-$source" timeout 90s "$xl" run "$pkg/examples/$source" > "$work/$source.jit.txt"
    run "parity-jit-$source" diff -u "$work/query.golden" "$work/$source.jit.txt"
done
run jit-example timeout 90s "$xl" run release/examples/extlibs/hcl.xl > "$work/hcl_xl.txt"
run jit-c-example timeout 90s "$xl" run release/examples/extlibs/hcl_c.c > "$work/hcl_direct_c.txt"
run parity-xl diff -u "$work/hcl_c.txt" "$work/hcl_xl.txt"
run parity-direct-c diff -u "$work/hcl_c.txt" "$work/hcl_direct_c.txt"
for name in aot_core boundaries allocations expression_strings; do
    run "aot-$name-build" timeout 120s "$xl" build "release/extlibs/hcl/test/$name.xl" \
        -o "$work/hcl_$name"
    run "aot-$name-run" timeout 90s "$work/hcl_$name"
done
run aot-c-expressions-build timeout 120s "$xl" build "$pkg/test/test_expression_strings.c" \
    -o "$work/hcl_c_expressions"
run aot-c-expressions-run timeout 90s "$work/hcl_c_expressions"
run aot-example-build timeout 120s "$xl" build release/examples/extlibs/hcl.xl \
    -o "$work/hcl_example_xl"
run aot-example-run timeout 45s "$work/hcl_example_xl" > "$work/hcl_aot.txt"
run parity-aot-xl diff -u "$work/hcl_c.txt" "$work/hcl_aot.txt"
run aot-c-example-build timeout 120s "$xl" build release/examples/extlibs/hcl_c.c \
    -o "$work/hcl_example_c_aot"
run aot-c-example-run timeout 45s "$work/hcl_example_c_aot" > "$work/hcl_c_aot.txt"
run parity-aot-c diff -u "$work/hcl_c.txt" "$work/hcl_c_aot.txt"
for source in query_comments.xl query_comments_c.c; do
    run "aot-$source-build" timeout 120s "$xl" build "$pkg/examples/$source" -o "$work/$source.exe"
    run "aot-$source-run" timeout 90s "$work/$source.exe" > "$work/$source.aot.txt"
    run "parity-aot-$source" diff -u "$work/query.golden" "$work/$source.aot.txt"
done
run fingerprint-after sha256sum "${inputs[@]}" > /dev/null
run inputs-unchanged diff -u "$work/fingerprint.stdout" "$work/fingerprint-after.stdout"
printf 'hcl Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
