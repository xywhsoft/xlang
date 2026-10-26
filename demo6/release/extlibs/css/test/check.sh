#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-css-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/css/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/css/test/runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/css/css_native.c -o "$work/css_native.o"
for source in test_css test_css_threads; do
    "$cc" "${flags[@]}" "release/extlibs/css/test/$source.c" \
        "$work/runtime.o" "$work/css_native.o" -no-pie -lm -ldl \
        -o "$work/$source"
    timeout 90s "$work/$source"
done
"$cc" "${flags[@]}" release/examples/extlibs/css_c.c \
    "$work/runtime.o" -no-pie -lm -ldl \
    -o "$work/css_example"
timeout 30s "$work/css_example" > "$work/example_c.txt"
timeout 30s release/xl run release/examples/extlibs/css_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for CSS XLang gate\n' >&2
    exit 1
fi
(
    cd release
    timeout 90s ./xl run ../test/cases/extlib_css.xl
    timeout 90s ./xl run extlibs/css/test/boundaries.xl
    timeout 90s ./xl run extlibs/css/test/allocations.xl
    timeout 90s ./xl run examples/extlibs/css.xl > "$work/example_xl.txt"
)
diff -u "$work/example_c.txt" "$work/example_xl.txt"
for name in aot_core boundaries allocations; do
    timeout 240s release/xl build "release/extlibs/css/test/$name.xl" \
        -o "$work/css_$name"
    timeout 90s "$work/css_$name"
done
timeout 240s release/xl build release/examples/extlibs/css.xl \
    -o "$work/css_example_xl"
timeout 45s "$work/css_example_xl" > "$work/example_aot.txt"
diff -u "$work/example_c.txt" "$work/example_aot.txt"
timeout 240s release/xl build release/examples/extlibs/css_c.c \
    -o "$work/css_example_c_aot"
timeout 45s "$work/css_example_c_aot" > "$work/example_c_aot.txt"
diff -u "$work/example_c.txt" "$work/example_c_aot.txt"
printf 'css Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
