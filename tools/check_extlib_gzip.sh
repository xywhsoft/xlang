#!/usr/bin/env bash
set -euo pipefail

# 复用固定快照；包层、XRT 编解码器和原生绑定均接受 Sanitizer 检查。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${GZIP_TEST_WORK:-"$root/build/test/gzip_linux_guard"}
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/gzip
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        test/cases/extlib_codec_file_allocation.c tools/check_extlib_gzip.sh \
        tools/check_extlib_gzip_interop.py release/xl \
        test/cases/extlib_gzip.xl test/cases/extlib_gzip_progress.xl \
        release/examples/extlibs/gzip.xl \
        release/examples/extlibs/gzip_basic.xl release/examples/extlibs/gzip_basic.expected.txt \
        release/examples/extlibs/gzip_c.c
    if [[ -n ${XRT_INCLUDE:-} ]]; then printf '%s\0' "$XRT_INCLUDE/xrt.h" "$XRT_INCLUDE/xruntime.h"; fi
}
mapfile -d '' inputs < <(input_paths | sort -zu)
includes=(-Ilib/xrt -Ilib/xruntime)
if [[ -n ${XRT_INCLUDE:-} ]]; then includes=("-I$XRT_INCLUDE"); fi
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror "${includes[@]}"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if [[ $result == 0 ]]; then printf 'Gzip Linux C ASan/UBSan and XLang gate: PASS\n'; fi
    exit "$result"
}
trap finish_gate EXIT
for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "$pkg/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'gzip: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'gzip: expected private-access diagnostic\n' >&2
        exit 1
    fi
done
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_DEFLATE -DXRT_MODULE_INFLATE -DXRT_MODULE_DIR -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c "$pkg/xrt_gzip.c" -o "$work/core.o"
"$cc" "${flags[@]}" -c "$pkg/gzip_native.c" -o "$work/native.o"
objects=("$work/core.o" "$work/native.o" "$work/runtime.o")
for name in test_gzip test_gzip_stream test_gzip_alloc; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/test/gzip_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
python3 tools/check_extlib_gzip_interop.py "$work/interop" "$work/interop-files" | tee "$work/interop.log"
"$cc" "${flags[@]}" "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/gzip_basic.expected.txt "$work/basic.log"
timeout 30s ./release/xl run "$pkg/examples/basic.c" > "$work/basic-direct.log"
diff -u "$work/basic.log" "$work/basic-direct.log"
timeout 30s ./release/xl run release/examples/extlibs/gzip_c.c > "$work/central-direct.log"
printf 'gzip C example: PASS\n' > "$work/central-expected.log"
diff -u "$work/central-expected.log" "$work/central-direct.log"
"$cc" "${flags[@]}" -DNDEBUG -Itest/cases "$pkg/test/file_allocation_dll.c" "${objects[@]}" -lm -ldl -o "$work/file-allocation"
timeout 90s "$work/file-allocation" "$work/files-$$" 2>&1 | tee "$work/file-allocation.log"
timeout 90s ./release/xl run "$pkg/test/file_ops.xl"
timeout 90s ./release/xl run "$pkg/test/error_dispatch.xl"
timeout 90s ./release/xl run test/cases/extlib_gzip_progress.xl
timeout 90s ./release/xl run release/examples/extlibs/gzip_basic.xl > "$work/basic-xl.log"
diff -u release/examples/extlibs/gzip_basic.expected.txt "$work/basic-xl.log"
diff -u "$work/basic.log" "$work/basic-xl.log"
timeout 90s ./release/xl run test/cases/extlib_gzip.xl
timeout 90s ./release/xl run release/examples/extlibs/gzip.xl > "$work/gzip-xl.log"
printf 'gzip example: PASS\n' > "$work/gzip-expected.log"
diff -u "$work/gzip-expected.log" "$work/gzip-xl.log"

for source in test/cases/extlib_gzip.xl test/cases/extlib_gzip_progress.xl \
    "$pkg/test/file_ops.xl" "$pkg/test/error_dispatch.xl"; do
    name=$(basename "${source%.xl}")
    timeout 90s ./release/xl build "$source" -o "$work/$name-aot"
    timeout 60s "$work/$name-aot"
done
for source in release/examples/extlibs/gzip.xl \
    release/examples/extlibs/gzip_basic.xl "$pkg/examples/basic.c"; do
    name=$(basename "${source%.*}")
    timeout 90s ./release/xl build "$source" -o "$work/$name-aot"
    timeout 60s "$work/$name-aot" > "$work/$name-aot.log"
    if [[ $name == gzip ]]; then
        diff -u "$work/gzip-expected.log" "$work/$name-aot.log"
    else
        diff -u release/examples/extlibs/gzip_basic.expected.txt \
            "$work/$name-aot.log"
    fi
done
