#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-lzma-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[lzma] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_ONCE -DXRT_MODULE_DIR_TEMP -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/lzma/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find release/extlibs/lzma -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/support/extlib_codec_runtime.c \
          tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh \
          test/cases/extlib_lzma.xl test/cases/extlib_lzma_progress.xl \
          release/examples/extlibs/lzma.xl
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -fsyntax-only release/extlibs/lzma/test/native_header.c
"$cc" "${flags[@]}" -DLZMA_CORE_FIRST -fsyntax-only release/extlibs/lzma/test/native_header.c

for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "release/extlibs/lzma/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'lzma: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'lzma: expected private-access diagnostic\n' >&2
        exit 1
    fi
done

"$cc" "${flags[@]}" -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/lzma/xrt_lzma.c -o "$work/core.o"
"$cc" "${flags[@]}" -c release/extlibs/lzma/xrt_lzma_common.c -o "$work/common.o"
"$cc" "${flags[@]}" -c release/extlibs/lzma/xrt_lzma_encode.c -o "$work/encode.o"
"$cc" "${flags[@]}" -c release/extlibs/lzma/xrt_lzma_decode.c -o "$work/decode.o"
"$cc" "${flags[@]}" -c release/extlibs/lzma/lzma_native.c -o "$work/bridge.o"
objects=("$work/runtime.o" "$work/core.o" "$work/common.o" "$work/encode.o" "$work/decode.o" "$work/bridge.o")
for name in 7zCrc 7zCrcOpt Alloc Bra Bra86 CpuArch Delta LzFind \
            LzmaDec LzmaEnc Lzma2Dec Lzma2Enc Sha256 Sha256Opt \
            Xz XzCrc64 XzCrc64Opt XzDec XzEnc; do
    "$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" \
        -c "release/extlibs/lzma/third_party/xz/$name.c" -o "$work/$name.o"
    objects+=("$work/$name.o")
done
"$cc" "${flags[@]}" release/extlibs/lzma/test/test_lzma.c \
    "${objects[@]}" -lm -ldl -o "$work/test_lzma"
"$cc" "${flags[@]}" release/extlibs/lzma/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
"$cc" "${flags[@]}" release/extlibs/lzma/test/test_native_options.c \
    "${objects[@]}" -lm -ldl -o "$work/test_native_options"
"$cc" "${flags[@]}" release/extlibs/lzma/examples/basic_c.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
"$cc" "${flags[@]}" release/extlibs/lzma/test/interop.c \
    "${objects[@]}" -lm -ldl -o "$work/interop"
timeout 90s "$work/test_lzma"
timeout 90s "$work/test_allocations"
timeout 90s "$work/test_native_options"
timeout 30s "$work/basic" > "$work/basic_c.txt"
timeout 30s ./release/xl run release/extlibs/lzma/examples/basic_c.c > "$work/basic_direct.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct.txt"
timeout 90s ./release/xl run release/extlibs/lzma/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/lzma/test/file_ops.xl
timeout 90s ./release/xl run release/examples/extlibs/lzma.xl > "$work/full_xl.txt"
timeout 90s ./release/xl run release/extlibs/lzma/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/lzma/test/text_transfer.xl
timeout 90s ./release/xl run release/extlibs/lzma/test/native_options.xl
timeout 90s ./release/xl run test/cases/extlib_lzma_progress.xl
timeout 90s ./release/xl run test/cases/extlib_lzma.xl
timeout 45s ./release/xl run release/extlibs/lzma/examples/basic.xl > "$work/basic_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
python3 release/extlibs/lzma/test/interop.py "$work/interop"
printf '[lzma] XLang AOT gate\n'
for name in aot_core boundaries file_ops allocations text_transfer native_options; do
    printf '[lzma] AOT %s\n' "$name"
    timeout 180s ./release/xl build "release/extlibs/lzma/test/$name.xl" \
        -o "$work/lzma_$name"
    timeout 90s "$work/lzma_$name"
done
for name in central progress; do
    printf '[lzma] AOT %s\n' "$name"
    source=test/cases/extlib_lzma.xl
    if [[ $name == progress ]]; then source=test/cases/extlib_lzma_progress.xl; fi
    timeout 180s ./release/xl build "$source" -o "$work/lzma_$name"
    timeout 90s "$work/lzma_$name"
done
for name in full basic; do
    printf '[lzma] AOT example %s\n' "$name"
    source=release/examples/extlibs/lzma.xl
    if [[ $name == basic ]]; then source=release/extlibs/lzma/examples/basic.xl; fi
    timeout 180s ./release/xl build "$source" -o "$work/lzma_$name"
    timeout 90s "$work/lzma_$name" > "$work/${name}_aot.txt"
done
timeout 180s ./release/xl build release/extlibs/lzma/examples/basic_c.c -o "$work/lzma_basic_c"
timeout 90s "$work/lzma_basic_c" > "$work/basic_c_aot.txt"
diff -u "$work/full_xl.txt" "$work/full_aot.txt"
diff -u "$work/basic_c.txt" "$work/basic_aot.txt"
diff -u "$work/basic_c.txt" "$work/basic_c_aot.txt"
bash "$root/tools/check_extlib_native_globals.sh" lzma
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'lzma Linux C ASan/UBSan, Python XZ, XLang JIT/AOT and parity gate: PASS\n'
