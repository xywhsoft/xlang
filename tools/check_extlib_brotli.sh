#!/usr/bin/env bash
set -euo pipefail

# 独立 C 验证；使用仓库固定快照，所有后端分配同样接受 ASan/UBSan 检查。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ -n ${BROTLI_TEST_WORK:-} ]]; then
    work=$BROTLI_TEST_WORK
    mkdir -p "$work"
else
    work_root="$root/build/test/brotli_linux_guard"
    mkdir -p "$work_root"
    work=$(mktemp -d "$work_root/run_XXXXXX")
fi
cc=${CC:-gcc}
pkg=release/extlibs/brotli
sources=("$pkg/xrt_brotli.c" "$pkg/brotli_native.c" "$pkg"/third_party/{common,dec,enc}/*.c)
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        test/cases/extlib_codec_file_allocation.c tools/check_extlib_brotli.sh \
        release/xl test/cases/extlib_brotli.xl test/cases/extlib_brotli_progress.xl \
        release/examples/extlibs/brotli.xl release/examples/extlibs/brotli_basic.xl \
        release/examples/extlibs/brotli_basic.expected.txt
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
    if [[ $result == 0 ]]; then printf 'Brotli Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'; fi
    exit "$result"
}
trap finish_gate EXIT
for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "$pkg/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'brotli: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'brotli: expected private-access diagnostic\n' >&2
        exit 1
    fi
done
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_DIR -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for index in "${!sources[@]}"; do
    object="$work/native-$index.o"
    "$cc" "${flags[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
done
for name in test_brotli test_brotli_alloc; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/brotli_basic.expected.txt "$work/basic.log"
"$cc" "${flags[@]}" -DNDEBUG "$pkg/test/file_allocation.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/file-allocation"
timeout 90s "$work/file-allocation" "$work/files-$$" 2>&1 | tee "$work/file-allocation.log"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'Brotli Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_brotli.xl
timeout 90s "$xl" run test/cases/extlib_brotli_progress.xl
timeout 90s "$xl" run "$pkg/test/error_dispatch.xl"
timeout 90s "$xl" run "$pkg/test/wire_fixture.xl"
timeout 90s "$xl" run release/examples/extlibs/brotli.xl > "$work/full_xl.log"
timeout 90s "$xl" run release/examples/extlibs/brotli_basic.xl > "$work/basic_xl.log"
diff -u release/examples/extlibs/brotli_basic.expected.txt "$work/basic_xl.log"
diff -u "$work/basic.log" "$work/basic_xl.log"
printf '[brotli] XLang AOT gate\n'
for name in extlib_brotli extlib_brotli_progress error_dispatch wire_fixture brotli brotli_basic; do
    printf '[brotli] AOT %s\n' "$name"
    case "$name" in
        extlib_*) source="test/cases/$name.xl" ;;
        error_dispatch|wire_fixture) source="$pkg/test/$name.xl" ;;
        *) source="release/examples/extlibs/$name.xl" ;;
    esac
    timeout 180s "$xl" build "$source" -o "$work/$name"
    timeout 90s "$work/$name" > "$work/${name}_aot.log"
done
diff -u "$work/full_xl.log" "$work/brotli_aot.log"
diff -u "$work/basic.log" "$work/brotli_basic_aot.log"
printf '[brotli] C file allocation AOT\n'
timeout 180s "$xl" build "$pkg/test/file_allocation.c" "${sources[@]}" \
    --inc-path lib/xrt --inc-path lib/xruntime -o "$work/file-allocation-aot"
timeout 90s "$work/file-allocation-aot" "$work/aot-files-$$" > "$work/file-allocation-aot.log"
