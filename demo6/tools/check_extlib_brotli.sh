#!/usr/bin/env bash
set -euo pipefail

# 独立 C 验证；使用仓库固定快照，所有后端分配同样接受 ASan/UBSan 检查。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
if [[ -n ${BROTLI_TEST_WORK:-} ]]; then
    work=$BROTLI_TEST_WORK
    mkdir -p "$work"
else
    work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-brotli-XXXXXX")
fi
cc=${CC:-gcc}
pkg=release/extlibs/brotli
sources=("$pkg/xrt_brotli.c" "$pkg/xrt_brotli_file.c" "$pkg/brotli_native.c" "$pkg"/third_party/{common,dec,enc}/*.c)
cc_path=$(command -v "$cc" || true)
input_paths() {
    find "$pkg" -type f -print0
    find release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        test/cases/extlib_codec_file_allocation.c test/support/extlib_codec_file_rollback.h tools/check_extlib_brotli.sh \
        release/xl release/libxl.so release/libxlc.so release/include/xrt_decl.h \
        release/include/xruntime_decl.h release/include/xl_extlib_abi.h \
        test/cases/extlib_brotli.xl test/cases/extlib_brotli_progress.xl \
        release/examples/extlibs/brotli.xl release/examples/extlibs/brotli_basic.xl \
        release/examples/extlibs/brotli_basic.expected.txt
    if [[ -n ${XRT_INCLUDE:-} ]]; then printf '%s\0' "$XRT_INCLUDE/xrt.h" "$XRT_INCLUDE/xruntime.h"; fi
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
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
    cd "$root" || exit 125
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
compile() { timeout 180s "$cc" "$@"; }
for order in ALONE CORE_FIRST NATIVE_FIRST XRT_FIRST; do
    compile "${flags[@]}" "-DBROTLI_$order" -fsyntax-only "$pkg/test/native_header.c"
done
compile "${flags[@]}" -DXRT_MODULE_DIR -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
core_objects=()
for index in "${!sources[@]}"; do
    object="$work/native-$index.o"
    compile "${flags[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
    if [[ ${sources[$index]} != "$pkg/brotli_native.c" ]]; then core_objects+=("$object"); fi
done
compile "${flags[@]}" -c "$pkg/test/xrt_file_runtime.c" -o "$work/xrt-file-runtime.o"
for name in files_c file_rollback_open; do
    compile "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${core_objects[@]}" "$work/xrt-file-runtime.o" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name" 2>&1 | tee "$work/$name.log"
done
for name in test_brotli test_brotli_alloc; do
    compile "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
compile "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
# 已有 Windows 工作树可能仍将黄金文件检出为 CRLF。
sed 's/\r$//' release/examples/extlibs/brotli_basic.expected.txt > "$work/basic.expected.txt"
diff -u "$work/basic.expected.txt" "$work/basic.log"
compile "${flags[@]}" -DNDEBUG "$pkg/test/file_allocation.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/file-allocation"
timeout 90s "$work/file-allocation" "$work/files-$$" 2>&1 | tee "$work/file-allocation.log"
compile "${flags[@]}" -DNDEBUG "$pkg/examples/files_c.c" "${core_objects[@]}" "$work/xrt-file-runtime.o" -lm -ldl -o "$work/files-example"
timeout 30s "$work/files-example" > "$work/files-example.log"
diff -u <(sed 's/\r$//' "$pkg/examples/files.expected.txt") "$work/files-example.log"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'Brotli Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
# WSL 挂载盘的语言编译可能静默失败；只把语言门禁所需输入暂存到原生目录。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" \
    "$stage/test/cases" "$stage/lib/xrt" "$stage/lib/xruntime"
cp -a release/xl release/libxl.so release/libxlc.so release/include "$stage/release/"
cp -a "$pkg" "$stage/release/extlibs/"
cp -a release/examples/extlibs/brotli.xl release/examples/extlibs/brotli_basic.xl \
    release/examples/extlibs/brotli_basic.expected.txt "$stage/release/examples/extlibs/"
cp -a test/cases/extlib_brotli.xl test/cases/extlib_brotli_progress.xl \
    test/cases/extlib_codec_file_allocation.c "$stage/test/cases/"
cp -a lib/xrt/xrt.h "$stage/lib/xrt/"
cp -a lib/xruntime/xruntime.h "$stage/lib/xruntime/"
cd "$stage"
timeout 90s "$xl" run test/cases/extlib_brotli.xl
timeout 90s "$xl" run test/cases/extlib_brotli_progress.xl
timeout 90s "$xl" run "$pkg/test/error_dispatch.xl"
timeout 90s "$xl" run "$pkg/test/wire_fixture.xl"
timeout 90s "$xl" run release/examples/extlibs/brotli.xl > "$work/full_xl.log"
timeout 90s "$xl" run release/examples/extlibs/brotli_basic.xl > "$work/basic_xl.log"
diff -u "$work/basic.expected.txt" "$work/basic_xl.log"
diff -u "$work/basic.log" "$work/basic_xl.log"
timeout 90s "$xl" run "$pkg/examples/files.xl" > "$work/files-xl.log"
diff -u "$work/files-example.log" "$work/files-xl.log"
timeout 90s "$xl" run "$pkg/examples/files_c.c" "${sources[@]}" \
    --inc-path lib/xrt --inc-path lib/xruntime > "$work/files-c-direct.log"
diff -u "$work/files-example.log" "$work/files-c-direct.log"
timeout 90s "$xl" run "$pkg/test/files_c.c" "${sources[@]}" --inc-path lib/xrt --inc-path lib/xruntime
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
for source in "$pkg/examples/files.xl" "$pkg/examples/files_c.c" "$pkg/test/files_c.c"; do
    binary="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    extra=()
    if [[ $source == *.c ]]; then extra=("${sources[@]}" --inc-path lib/xrt --inc-path lib/xruntime); fi
    timeout 180s "$xl" build "$source" "${extra[@]}" -o "$binary"
    timeout 90s "$binary" > "$binary.log"
    if [[ $source == "$pkg/examples/"* ]]; then diff -u "$work/files-example.log" "$binary.log"; fi
done

cd "$root"
compile "${flags[@]}" -DNDEBUG "$pkg/test/file_rollback_closed.c" "${core_objects[@]}" "$work/xrt-file-runtime.o" -lm -ldl -o "$work/rollback-closed"
if timeout 90s "$work/rollback-closed" "$work/closed-files" > "$work/rollback-closed.log" 2>&1; then closed_result=0
else closed_result=$?; cat "$work/rollback-closed.log"; fi
cd "$stage"

printf '[brotli] Top-level native object lifecycle\n'
timeout 90s "$xl" run "$pkg/test/top_level.xl"
timeout 180s "$xl" build "$pkg/test/top_level.xl" -o "$work/top-level"
timeout 90s "$work/top-level" 2>&1 | tee "$work/top-level.log"
test "$closed_result" -eq 0
