#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-tabulate-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/tabulate/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

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
printf 'tabulate Linux C ASan/UBSan and XLang parity gate: PASS\n'
