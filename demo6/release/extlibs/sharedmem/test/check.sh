#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sharedmem-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/sharedmem
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/sharedmem/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" \
    release/extlibs/sharedmem/sharedmem_native.c \
    release/extlibs/sharedmem/test/test_sharedmem.c "$work/xrt_runtime.o" \
    -no-pie -lm -ldl -lrt -o "$work/test_sharedmem"
timeout 120s "$work/test_sharedmem"
"$cc" "${flags[@]}" \
    release/extlibs/sharedmem/sharedmem_native.c \
    release/extlibs/sharedmem/examples/sharedmem_c.c "$work/xrt_runtime.o" \
    -no-pie -lm -ldl -lrt -o "$work/sharedmem_example"
timeout 30s "$work/sharedmem_example" > "$work/c_example.txt"
timeout 45s ./release/xl run release/extlibs/sharedmem/examples/sharedmem_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_sharedmem.xl
timeout 90s ./release/xl run release/extlibs/sharedmem/test/boundaries.xl
if timeout 90s ./release/xl check \
    release/extlibs/sharedmem/test/private_native_negative.xl \
    > "$work/private_native.txt" 2>&1; then
    printf 'sharedmem: raw native writer must be private\n' >&2
    exit 1
fi
if ! grep -Fq 'writeNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'sharedmem: expected private-access diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/sharedmem.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
printf 'sharedmem Linux C ASan/UBSan and XLang parity gate: PASS\n'
