#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-fuzzy-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[fuzzy] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
cd "$root"

cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/fuzzy -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/fuzzy release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_fuzzy.xl release/examples/extlibs/fuzzy.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/fuzzy/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/fuzzy/test/xrt_runtime.c -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_fuzzy.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_fuzzy"
timeout 90s "$work/test_fuzzy"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_unterminated.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_unterminated"
timeout 90s "$work/test_unterminated"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"

"$cc" "${flags[@]}" release/extlibs/fuzzy/fuzzy_native.c \
    release/extlibs/fuzzy/examples/fuzzy_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'fuzzy: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/fuzzy/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/fuzzy/test/allocations.xl
timeout 90s "$xl" run release/extlibs/fuzzy/test/owned_results.xl
timeout 90s "$xl" run test/cases/extlib_fuzzy.xl
timeout 90s "$xl" cgen release/extlibs/fuzzy/test/owned_probe.xl --emit "$work/owned_probe.c"
count=$(grep -Ec '[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xfuzzy_best[[:space:]]*\(' \
    "$work/owned_probe.c")
if [[ $count != 1 ]] || grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'fuzzy: whole-call probe must borrow inputs and adopt results without copying\n' >&2
    exit 1
fi
timeout 30s "$xl" run release/extlibs/fuzzy/examples/fuzzy_c.c > "$work/example_direct.txt"
timeout 90s "$xl" run release/examples/extlibs/fuzzy.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_direct.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
printf 'levenshtein(kitten, sitting): 3\nratio(hello, hallo): 80\nscore(fb, foobar): 7\nisMatch(xyz, foobar): false\nbest(pc, ...): lib/parser.c\nbest(abc, ...): abc\nfuzzy example: PASS\n' > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/example_c.txt"

timeout 90s "$xl" build test/cases/extlib_fuzzy.xl -o "$work/fuzzy_core"
timeout 30s "$work/fuzzy_core"
for case_name in boundaries allocations owned_results; do
    timeout 90s "$xl" build "release/extlibs/fuzzy/test/$case_name.xl" -o "$work/fuzzy_$case_name"
    timeout 30s "$work/fuzzy_$case_name"
done
timeout 90s "$xl" build release/examples/extlibs/fuzzy.xl -o "$work/fuzzy_xl_aot"
timeout 30s "$work/fuzzy_xl_aot" > "$work/xl_aot.txt"
diff -u "$work/expected.txt" "$work/xl_aot.txt"
timeout 90s "$xl" build release/extlibs/fuzzy/examples/fuzzy_c.c -o "$work/fuzzy_c_aot"
timeout 30s "$work/fuzzy_c_aot" > "$work/c_aot.txt"
diff -u "$work/expected.txt" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'fuzzy Linux C ASan/UBSan, fixed output and bilingual AOT gate: PASS\n'
