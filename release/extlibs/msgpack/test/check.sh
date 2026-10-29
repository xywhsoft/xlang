#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-msgpack-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/msgpack -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/msgpack/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/msgpack/msgpack_native.c \
    -o "$work/msgpack_native.o"
for name in test_msgpack test_boundaries test_allocations; do
    "$cc" "${flags[@]}" "release/extlibs/msgpack/test/$name.c" \
        "$work/msgpack_native.o" "$work/xrt_runtime.o" -lm -ldl \
        -o "$work/$name"
    timeout 90s "$work/$name"
done
"$cc" "${flags[@]}" release/extlibs/msgpack/examples/msgpack_c.c \
    "$work/xrt_runtime.o" -lm -ldl \
    -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
diff -u release/extlibs/msgpack/examples/basic.expected.txt "$work/c.txt"
if timeout 30s "$work/example" extra extra > /dev/null; then
    printf 'msgpack C example accepted extra arguments\n' >&2
    exit 1
else
    test "$?" -eq 1
fi
if timeout 30s "$work/example" "$work/missing/output.bin" > /dev/null; then
    printf 'msgpack C example accepted an unwritable output path\n' >&2
    exit 1
else
    test "$?" -eq 1
fi
timeout 30s ./release/xl run release/extlibs/msgpack/examples/msgpack_c.c \
    > "$work/direct_c.txt"
diff -u "$work/c.txt" "$work/direct_c.txt"
timeout 45s ./release/xl run release/extlibs/msgpack/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/msgpack/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/msgpack/examples/msgpack.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
printf 'msgpack Linux ASan/UBSan gate: PASS\n'
