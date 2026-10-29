#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-miniz-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_DEFLATE -DXRT_MODULE_INFLATE
       -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -Irelease/extlibs/miniz/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "release/extlibs/miniz/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'miniz: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'miniz: expected private-access diagnostic\n' >&2
        exit 1
    fi
done

"$cc" "${flags[@]}" -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/miniz/xrt_miniz.c -o "$work/core.o"
"$cc" "${flags[@]}" -c release/extlibs/miniz/miniz_native.c -o "$work/bridge.o"
objects=("$work/runtime.o" "$work/core.o" "$work/bridge.o")
"$cc" "${flags[@]}" -DNDEBUG release/extlibs/miniz/test/test_miniz.c \
    "${objects[@]}" -lm -ldl -o "$work/test_miniz"
"$cc" "${flags[@]}" release/extlibs/miniz/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
"$cc" "${flags[@]}" release/extlibs/miniz/examples/basic.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 90s "$work/test_miniz"
timeout 30s "$work/test_allocations"
timeout 30s "$work/basic" > "$work/basic_c.txt"
timeout 90s ./release/xl run release/extlibs/miniz/examples/basic.c > "$work/basic_direct.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_miniz.xl
timeout 90s ./release/xl run test/cases/extlib_miniz_progress.xl
timeout 90s ./release/xl run release/extlibs/miniz/test/file_ops.xl > "$work/file_ops_jit.txt"
timeout 90s ./release/xl run release/examples/extlibs/miniz.xl > "$work/full_xl.txt"
timeout 90s ./release/xl run release/extlibs/miniz/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/miniz/test/text_transfer.xl
timeout 90s ./release/xl run release/examples/extlibs/miniz_basic.xl > "$work/basic_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
hex=$(/usr/bin/timeout 30s "$work/test_miniz" --emit)
python3 -c 'import sys,zlib; assert zlib.decompress(bytes.fromhex(sys.argv[1])) == b"abc"' "$hex"
printf '[miniz] XLang AOT gate\n'
for name in central progress file_ops allocations text_transfer basic full; do
    printf '[miniz] AOT %s\n' "$name"
    case "$name" in
        central) source=test/cases/extlib_miniz.xl ;;
        progress) source=test/cases/extlib_miniz_progress.xl ;;
        basic) source=release/examples/extlibs/miniz_basic.xl ;;
        full) source=release/examples/extlibs/miniz.xl ;;
        *) source="release/extlibs/miniz/test/$name.xl" ;;
    esac
    timeout 180s ./release/xl build "$source" -o "$work/miniz_$name"
    timeout 90s "$work/miniz_$name" > "$work/${name}_aot.txt"
done
diff -u "$work/file_ops_jit.txt" "$work/file_ops_aot.txt"
diff -u "$work/basic_c.txt" "$work/basic_aot.txt"
diff -u "$work/full_xl.txt" "$work/full_aot.txt"
printf 'miniz Linux C ASan/UBSan, Python zlib, XLang JIT/AOT and parity gate: PASS\n'
