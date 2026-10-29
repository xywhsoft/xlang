#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-hcl-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

printf '[hcl] C sanitizer gate\n'
"$cc" "${flags[@]}" -fsyntax-only release/extlibs/hcl/test/abi_contract.c
"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/extlibs/hcl/test/test_hcl.c -o "$work/test_hcl.o"
"$cc" "${flags[@]}" -c release/extlibs/hcl/hcl_native.c \
    -o "$work/hcl_native.o"
"$cc" "${flags[@]}" -no-pie "$work/test_hcl.o" "$work/hcl_native.o" \
    -lm -ldl -o "$work/test_hcl"
timeout 90s "$work/test_hcl"

"$cc" "${flags[@]}" -DXRT_IMPLEMENTATION -c \
    release/examples/extlibs/hcl_c.c -o "$work/hcl_example.o"
"$cc" "${flags[@]}" -no-pie "$work/hcl_example.o" \
    -lm -ldl -o "$work/hcl_example"
timeout 90s "$work/hcl_example" > "$work/hcl_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'hcl Linux gate: release/xl is required\n' >&2
    exit 1
fi
printf '[hcl] XLang JIT gate\n'
timeout 90s "$xl" run test/cases/extlib_hcl.xl
timeout 90s "$xl" run release/extlibs/hcl/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/hcl/test/allocations.xl
timeout 90s "$xl" run release/examples/extlibs/hcl.xl > "$work/hcl_xl.txt"
timeout 90s "$xl" run release/examples/extlibs/hcl_c.c > "$work/hcl_direct_c.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_xl.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_direct_c.txt"
printf '[hcl] XLang AOT gate\n'
for name in aot_core boundaries allocations; do
    timeout 120s "$xl" build "release/extlibs/hcl/test/$name.xl" \
        -o "$work/hcl_$name"
    timeout 90s "$work/hcl_$name"
done
timeout 120s "$xl" build release/examples/extlibs/hcl.xl \
    -o "$work/hcl_example_xl"
timeout 45s "$work/hcl_example_xl" > "$work/hcl_aot.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_aot.txt"
timeout 120s "$xl" build release/examples/extlibs/hcl_c.c \
    -o "$work/hcl_example_c_aot"
timeout 45s "$work/hcl_example_c_aot" > "$work/hcl_c_aot.txt"
diff -u "$work/hcl_c.txt" "$work/hcl_c_aot.txt"
printf 'hcl Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
