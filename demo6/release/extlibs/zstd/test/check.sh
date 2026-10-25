#!/usr/bin/env bash
set -euo pipefail

mode=${1:-}
if [[ -n $mode && $mode != --c-only ]]; then
    printf 'usage: %s [--c-only]\n' "$0" >&2
    exit 2
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zstd-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/zstd/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

if [[ $mode != --c-only ]]; then
    for kind in compressor decompressor; do
        case="private_${kind}_negative"
        if timeout 90s ./release/xl check "release/extlibs/zstd/test/$case.xl" > "$work/$case.txt" 2>&1; then
            printf 'zstd: %s must be rejected\n' "$case" >&2
            exit 1
        fi
        if ! grep -Fq 'updateNative' "$work/$case.txt" ||
           ! grep -Fq 'access' "$work/$case.txt"; then
            cat "$work/$case.txt" >&2
            printf 'zstd: expected private-access diagnostic\n' >&2
            exit 1
        fi
    done
fi

"$cc" "${flags[@]}" -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/zstd/xrt_zstd.c -o "$work/core.o"
"$cc" "${flags[@]}" -c release/extlibs/zstd/zstd_native.c -o "$work/bridge.o"
"$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" \
    -c release/extlibs/zstd/third_party/zstd.c -o "$work/vendor.o"
objects=("$work/runtime.o" "$work/core.o" "$work/bridge.o" "$work/vendor.o")
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_zstd.c \
    "${objects[@]}" -lm -ldl -o "$work/test_zstd"
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_file.c \
    "${objects[@]}" -lm -ldl -o "$work/test_file"
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
"$cc" "${flags[@]}" release/extlibs/zstd/examples/basic_c.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 90s "$work/test_zstd"
timeout 30s "$work/test_file"
timeout 30s "$work/test_allocations"
timeout 30s "$work/basic" > "$work/basic_c.txt"
if [[ $mode == --c-only ]]; then
    printf 'zstd Linux C ASan/UBSan gate: PASS\n'
    exit 0
fi
timeout 90s ./release/xl run release/extlibs/zstd/examples/basic_c.c > "$work/basic_direct.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_zstd_progress.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/wire_fixture.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/file_ops.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/concat_boundary.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/error_dispatch.xl
timeout 45s ./release/xl run release/extlibs/zstd/examples/basic.xl > "$work/basic_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
timeout 90s ./release/xl run release/examples/extlibs/zstd.xl > "$work/teaching.txt"
grep -Fq 'zstd example: PASS' "$work/teaching.txt"
printf 'zstd Linux ASan/UBSan gate: PASS\n'
