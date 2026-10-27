#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-memcached-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/memcached -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c release/extlibs/memcached/test/xrt_runtime.c -o "$work/xrt.o"
sources=(release/extlibs/memcached/memcached_request.c
         release/extlibs/memcached/memcached_response.c)
"$cc" "${flags[@]}" release/extlibs/memcached/test/test_memcached.c \
    "${sources[@]}" "$work/xrt.o" -lm -ldl -o "$work/test_memcached"
timeout 30s "$work/test_memcached"
"$cc" "${flags[@]}" release/extlibs/memcached/examples/basic_c.c \
    "${sources[@]}" "$work/xrt.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
sed 's/\r$//' release/extlibs/memcached/examples/basic.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'memcached Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/memcached/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/memcached/test/loopback.xl
timeout 90s "$xl" run release/extlibs/memcached/examples/basic.xl > "$work/xl.txt"
diff -u "$work/expected.txt" "$work/xl.txt"
for item in boundaries loopback basic; do
    source="release/extlibs/memcached/test/$item.xl"
    if [[ "$item" == basic ]]; then source=release/extlibs/memcached/examples/basic.xl; fi
    timeout 180s "$xl" build "$source" -o "$work/$item"
    timeout 30s "$work/$item" > "$work/$item.txt"
done
diff -u "$work/expected.txt" "$work/basic.txt"
timeout 180s "$xl" build release/extlibs/memcached/examples/basic_c.c \
    "${sources[@]}" --inc-path lib/xrt -o "$work/c_aot"
timeout 30s "$work/c_aot" > "$work/c_aot.txt"
diff -u "$work/expected.txt" "$work/c_aot.txt"
printf 'memcached Linux C ASan/UBSan and XLang gate: PASS\n'
