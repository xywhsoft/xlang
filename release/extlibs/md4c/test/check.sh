#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-md4c-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/include -Irelease/extlibs/md4c
       -fsanitize=address,undefined -fno-omit-frame-pointer -pthread)
native=(release/extlibs/md4c/md4c_native.c
        release/extlibs/md4c/third_party/md4c/md4c.c
        release/extlibs/md4c/third_party/md4c/md4c-html.c
        release/extlibs/md4c/third_party/md4c/entity.c)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/md4c/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" "${native[@]}" \
    release/extlibs/md4c/test/test_md4c.c \
    "$work/xrt_runtime.o" \
    -lm -o "$work/test_md4c"
timeout 90s "$work/test_md4c"

"$cc" "${flags[@]}" release/extlibs/md4c/examples/md4c_c.c \
    "$work/xrt_runtime.o" \
    -lm -o "$work/md4c_example"
timeout 30s "$work/md4c_example" > "$work/example.html"
grep -q '<h1>Title</h1>' "$work/example.html"
grep -q '<strong>xlang</strong>' "$work/example.html"

if [[ ! -x ./release/xl ]]; then
    printf 'missing Linux release/xl for md4c XLang gate\n' >&2
    exit 1
fi
timeout 30s ./release/xl run release/extlibs/md4c/examples/md4c_c.c \
    > "$work/example_direct_c.html"
diff -u "$work/example.html" "$work/example_direct_c.html"
timeout 90s ./release/xl run release/extlibs/md4c/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/md4c/test/allocations.xl
timeout 90s ./release/xl run test/cases/extlib_md4c.xl
timeout 90s ./release/xl run release/extlibs/md4c/examples/md4c.xl > "$work/example_xl.html"
diff -u "$work/example.html" "$work/example_xl.html"
timeout 120s ./release/xl build release/extlibs/md4c/test/boundaries.xl \
    -o "$work/md4c_boundaries"
timeout 90s "$work/md4c_boundaries"
timeout 120s ./release/xl build release/extlibs/md4c/test/allocations.xl \
    -o "$work/md4c_allocations"
timeout 90s "$work/md4c_allocations"
timeout 120s ./release/xl build release/extlibs/md4c/examples/md4c.xl \
    -o "$work/md4c_example_xl"
timeout 45s "$work/md4c_example_xl" > "$work/example_aot.html"
diff -u "$work/example.html" "$work/example_aot.html"
timeout 120s ./release/xl build release/examples/extlibs/md4c.xl \
    -o "$work/md4c_public_xl"
timeout 45s "$work/md4c_public_xl"
timeout 120s ./release/xl build release/extlibs/md4c/examples/md4c_c.c \
    -o "$work/md4c_example_c_aot"
timeout 45s "$work/md4c_example_c_aot" > "$work/example_c_aot.html"
diff -u "$work/example.html" "$work/example_c_aot.html"
printf 'md4c Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
