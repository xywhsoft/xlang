#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-miniz-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[miniz] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_DEFLATE -DXRT_MODULE_INFLATE
       -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -Irelease/extlibs/miniz/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find release/extlibs/miniz -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/support/extlib_codec_runtime.c \
          test/cases/extlib_miniz.xl test/cases/extlib_miniz_progress.xl \
          release/examples/extlibs/miniz.xl release/examples/extlibs/miniz_basic.xl
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
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
"$cc" "${flags[@]}" release/extlibs/miniz/test/test_io_errors.c \
    "${objects[@]}" -lm -ldl -o "$work/test_io_errors"
"$cc" "${flags[@]}" release/extlibs/miniz/examples/basic.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
"$cc" "${flags[@]}" release/extlibs/miniz/examples/streaming_c.c \
    "${objects[@]}" -lm -ldl -o "$work/streaming_c"
timeout 90s "$work/test_miniz"
timeout 30s "$work/test_allocations"
timeout 30s "$work/test_io_errors"
timeout 90s ./release/xl run release/extlibs/miniz/test/test_io_errors.c
timeout 30s "$work/streaming_c" > "$work/streaming_gcc.txt"
timeout 90s ./release/xl run release/extlibs/miniz/examples/streaming_c.c > "$work/streaming_direct_c.txt"
timeout 90s ./release/xl run release/extlibs/miniz/examples/streaming.xl > "$work/streaming_xl.txt"
diff -u "$work/streaming_gcc.txt" "$work/streaming_direct_c.txt"
diff -u "$work/streaming_gcc.txt" "$work/streaming_xl.txt"
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
for source in streaming.xl streaming_c.c; do
    target="$work/${source//./_}"
    timeout 180s ./release/xl build "release/extlibs/miniz/examples/$source" -o "$target"
    timeout 90s "$target" > "$work/${source//./_}.txt"
    diff -u "$work/streaming_gcc.txt" "$work/${source//./_}.txt"
done
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
printf 'miniz Linux C ASan/UBSan, Python zlib, XLang JIT/AOT and parity gate: PASS\n'
