#!/usr/bin/env bash
set -euo pipefail

# C sanitizer 与独立 XLang 文件用例；完整语言回归仍受共享 XLIR 问题阻断。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${BZ2_TEST_WORK:-"$root/build/test/bz2_linux_guard"}
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/bz2
vendor=release/extlibs/_bzip2/third_party/bzip2
includes=(-Ilib/xrt -Ilib/xruntime)
if [[ -n ${XRT_INCLUDE:-} ]]; then includes=("-I$XRT_INCLUDE"); fi
input_paths() {
    find "$pkg" release/extlibs/_bzip2 -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        tools/check_extlib_bz2.sh tools/check_extlib_bz2_interop.py \
        release/examples/extlibs/bz2_basic.expected.txt release/examples/extlibs/bz2_files.expected.txt
    if [[ -n ${XRT_INCLUDE:-} ]]; then printf '%s\0' "$XRT_INCLUDE/xrt.h" "$XRT_INCLUDE/xruntime.h"; fi
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror "${includes[@]}" "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
# 失败也记录退出码和输入漂移，不能复用上一次通过留下的验收结论。
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if [[ $result == 0 ]]; then printf 'BZ2 Linux C ASan/UBSan and XLang file gate: PASS\n'; fi
    exit "$result"
}
trap finish_gate EXIT
for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "$pkg/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'bz2: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'bz2: expected private-access diagnostic\n' >&2
        exit 1
    fi
done
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_DIR -DXRT_MODULE_DIR_TEMP -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_PATH \
    -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
for source in xrt_bz2 xrt_bz2_file bz2_native; do
    "$cc" "${flags[@]}" -c "$pkg/$source.c" -o "$work/$source.o"
    objects+=("$work/$source.o")
done
for source in blocksort huffman crctable randtable compress decompress bzlib; do
    "$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" -c "$vendor/$source.c" -o "$work/vendor-$source.o"
    objects+=("$work/vendor-$source.o")
done
for name in test_bz2 test_bz2_boundaries test_bz2_stream test_bz2_alloc; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/test/bz2_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
python3 tools/check_extlib_bz2_interop.py "$work/interop" "$work/interop-files" | tee "$work/interop.log"
for name in basic files; do
    "$cc" "${flags[@]}" "$pkg/examples/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.log"
    diff -u "release/examples/extlibs/bz2_$name.expected.txt" "$work/$name.log"
    timeout 30s ./release/xl run "$pkg/examples/$name.c" > "$work/$name-direct.log"
    diff -u "$work/$name.log" "$work/$name-direct.log"
done
timeout 30s ./release/xl run release/examples/extlibs/bz2_c.c > "$work/central-direct.log"
printf 'bz2 C example: PASS\n' > "$work/central-expected.log"
diff -u "$work/central-expected.log" "$work/central-direct.log"
"$cc" "${flags[@]}" -DNDEBUG "$pkg/test/test_bz2_files.c" "${objects[@]}" -lm -ldl -o "$work/test_bz2_files"
timeout 90s "$work/test_bz2_files" "$work/files-$$" 2>&1 | tee "$work/test_bz2_files.log"
timeout 90s ./release/xl run "$pkg/test/file_ops.xl"
timeout 90s ./release/xl run release/examples/extlibs/bz2_files.xl > "$work/files_xl.log"
diff -u release/examples/extlibs/bz2_files.expected.txt "$work/files_xl.log"
diff -u "$work/files.log" "$work/files_xl.log"
