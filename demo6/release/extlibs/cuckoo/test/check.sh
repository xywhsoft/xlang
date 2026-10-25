#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cuckoo-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/cuckoo/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/cuckoo/include -DXRT_MODULE_HASH64 \
    release/extlibs/cuckoo/cuckoo_native.c

"$cc" "${flags[@]}" -c release/extlibs/cuckoo/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/cuckoo/cuckoo_native.c \
    release/extlibs/cuckoo/test/test_cuckoo.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_cuckoo"
timeout 90s "$work/test_cuckoo"
"$cc" "${flags[@]}" release/extlibs/cuckoo/cuckoo_native.c \
    release/extlibs/cuckoo/examples/cuckoo.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/cuckoo_example"
timeout 90s "$work/cuckoo_example" > "$work/c_example.txt"
diff -u release/extlibs/cuckoo/examples/cuckoo.expected.txt "$work/c_example.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'cuckoo Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_cuckoo.xl
timeout 90s "$xl" run test/cases/extlib_cuckoo_boundaries.xl
timeout 90s "$xl" run release/extlibs/cuckoo/test/high_load.xl
timeout 90s "$xl" run release/extlibs/cuckoo/test/allocations.xl
if timeout 90s "$xl" check release/extlibs/cuckoo/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'cuckoo: raw native add must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'addNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'cuckoo: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/examples/extlibs/cuckoo.xl > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 90s "$xl" run release/extlibs/cuckoo/examples/cuckoo.c > "$work/direct_c_example.txt"
diff -u "$work/c_example.txt" "$work/direct_c_example.txt"
printf 'cuckoo Linux C ASan/UBSan and XLang parity gate: PASS\n'
