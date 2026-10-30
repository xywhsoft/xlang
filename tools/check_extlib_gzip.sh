#!/usr/bin/env bash
set -Eeuo pipefail

# 复用固定快照；包层、XRT 编解码器和原生绑定均接受 Sanitizer 检查。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${GZIP_TEST_WORK:-$(mktemp -d "${TMPDIR:-/tmp}/xlang-gzip-XXXXXX")}
mkdir -p "$work"
cc=${CC:-gcc}
cc_path=$(command -v "$cc")
cc1_path=$(timeout 30s "$cc" -print-prog-name=cc1)
python_path=$(command -v python3)
pkg=release/extlibs/gzip
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        test/support/extlib_codec_io_errors.h test/support/extlib_native_codec_file_rollback.h \
        test/cases/extlib_codec_file_allocation.c tools/check_extlib_gzip.sh \
        tools/check_extlib_gzip_interop.py release/xl \
        release/libxl.so release/libxlc.so release/include/xrt_decl.h \
        release/include/xruntime_decl.h release/include/xl_extlib_abi.h \
        tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh \
        test/cases/extlib_gzip.xl test/cases/extlib_gzip_progress.xl \
        release/examples/extlibs/gzip.xl \
        release/examples/extlibs/gzip_basic.xl release/examples/extlibs/gzip_basic.expected.txt \
        release/examples/extlibs/gzip_c.c
    find release/include -type f -print0
    printf '%s\0' "$cc_path" "$cc1_path" "$python_path"
    if [[ -n ${XRT_INCLUDE:-} ]]; then printf '%s\0' "$XRT_INCLUDE/xrt.h" "$XRT_INCLUDE/xruntime.h"; fi
}
mapfile -d '' inputs < <(input_paths | sort -zu)
includes=(-Ilib/xrt -Ilib/xruntime)
if [[ -n ${XRT_INCLUDE:-} ]]; then includes=("-I$XRT_INCLUDE"); fi
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_DIR_TEMP -O1 -g -Wall -Wextra -Werror "${includes[@]}"
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
# Windows 检出的黄金文本可为 CRLF；只规范化期望行尾，不改实际输出。
golden_output() {
    diff -u <(sed 's/\r$//' "$1") "$2"
}
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
    if [[ $result == 0 ]]; then printf 'Gzip Linux C ASan/UBSan and XLang gate: PASS\n'
    else printf '[gzip] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }
for kind in compressor decompressor; do
    case="private_${kind}_negative"
    status=0
    timeout 90s ./release/xl check "$pkg/test/$case.xl" > "$work/$case.txt" 2>&1 || status=$?
    if ((status != 1)); then
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
timeout 30s "$cc" --version > "$work/compiler.txt"
for order in ALONE CORE_FIRST NATIVE_FIRST XRT_FIRST; do
    compile "-DGZIP_$order" -fsyntax-only "$pkg/test/native_header.c"
done
compile -DXRT_MODULE_DEFLATE -DXRT_MODULE_INFLATE -DXRT_MODULE_DIR -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
compile -c "$pkg/xrt_gzip.c" -o "$work/core.o"
compile -c "$pkg/gzip_native.c" -o "$work/native.o"
objects=("$work/core.o" "$work/native.o" "$work/runtime.o")
for name in test_gzip test_gzip_stream test_gzip_alloc; do
    compile -DNDEBUG "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
cargs=("$pkg/xrt_gzip.c" "$pkg/gzip_native.c" --inc-path lib/xrt --inc-path lib/xruntime)
for name in io_errors_c file_rollback_c; do
    compile "$pkg/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
    extra=()
    if [[ $name != io_errors_c ]]; then extra=("${cargs[@]}"); fi
    timeout 90s ./release/xl run "$pkg/test/$name.c" "${extra[@]}"
    timeout 180s ./release/xl build "$pkg/test/$name.c" "${extra[@]}" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot"
done
compile "$pkg/test/gzip_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
timeout 180s python3 tools/check_extlib_gzip_interop.py "$work/interop" "$work/interop-files" | tee "$work/interop.log"
compile "$pkg/examples/basic.c" "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
golden_output release/examples/extlibs/gzip_basic.expected.txt "$work/basic.log"
timeout 30s ./release/xl run "$pkg/examples/basic.c" > "$work/basic-direct.log"
diff -u "$work/basic.log" "$work/basic-direct.log"
timeout 30s ./release/xl run release/examples/extlibs/gzip_c.c > "$work/central-direct.log"
printf 'gzip C example: PASS\n' > "$work/central-expected.log"
diff -u "$work/central-expected.log" "$work/central-direct.log"
compile -DNDEBUG -Itest/cases "$pkg/test/file_allocation_dll.c" "${objects[@]}" -lm -ldl -o "$work/file-allocation"
timeout 90s "$work/file-allocation" "$work/files-$$" 2>&1 | tee "$work/file-allocation.log"
timeout 90s ./release/xl run "$pkg/test/file_allocation_dll.c" "${cargs[@]}" --inc-path test/cases -- "$work/jit-files"
timeout 180s ./release/xl build "$pkg/test/file_allocation_dll.c" "${cargs[@]}" --inc-path test/cases -o "$work/file-allocation.aot"
timeout 90s "$work/file-allocation.aot" "$work/aot-files"
timeout 90s ./release/xl run "$pkg/test/file_ops.xl"
timeout 90s ./release/xl run "$pkg/test/error_dispatch.xl"
timeout 90s ./release/xl run test/cases/extlib_gzip_progress.xl
timeout 90s ./release/xl run release/examples/extlibs/gzip_basic.xl > "$work/basic-xl.log"
golden_output release/examples/extlibs/gzip_basic.expected.txt "$work/basic-xl.log"
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
        golden_output release/examples/extlibs/gzip_basic.expected.txt \
            "$work/$name-aot.log"
    fi
done
timeout 180s ./release/xl build release/examples/extlibs/gzip_c.c -o "$work/central-c.aot"
timeout 90s "$work/central-c.aot" > "$work/central-c.aot.txt"
diff -u "$work/central-expected.log" "$work/central-c.aot.txt"
for name in extlib_gzip extlib_gzip_progress; do
    timeout 90s ./release/xl cgen "test/cases/$name.xl" --emit "$work/$name.c"
done
timeout 30s python3 - "$pkg" "$work" <<'PY'
import re, sys
from pathlib import Path
pkg, work = map(Path, sys.argv[1:])
source = (pkg / 'src/gzip.xl').read_text(encoding='utf-8-sig')
expected = set(re.findall(r'extern "(xgzip_[a-z_]+)"', source))
declarations = sorted({line for name in ('extlib_gzip', 'extlib_gzip_progress')
    for line in (work / (name + '.c')).read_text(encoding='utf-8-sig').splitlines()
    if line.startswith('extern ') and re.search(r'\bxgzip_[a-z_]+\(', line)})
actual = set(re.findall(r'\bxgzip_[a-z_]+(?=\()', '\n'.join(declarations)))
if expected != actual or len(declarations) != len(expected):
    raise RuntimeError('incomplete generated gzip ABI coverage')
text = '#include "gzip_native.h"\n' + '\n'.join(declarations) + '\n'
(work / 'abi.c').write_text(text, encoding='utf-8')
wrong = text.replace('void* xgzip_compress(', 'int xgzip_compress(')
if wrong == text: raise RuntimeError('missing ABI negative anchor')
(work / 'wrong-abi.c').write_text(wrong, encoding='utf-8')
print(f'gzip generated ABI: {len(expected)} declarations')
PY
compile -I"$pkg/include" -fsyntax-only "$work/abi.c"
status=0
compile -I"$pkg/include" -fsyntax-only "$work/wrong-abi.c" > "$work/wrong-abi.txt" 2>&1 || status=$?
test "$status" -eq 1 && grep -Fq 'conflicting types' "$work/wrong-abi.txt" && grep -Fq 'xgzip_compress' "$work/wrong-abi.txt"
compile "$pkg/examples/file_transaction_c.c" "${objects[@]}" -lm -ldl -o "$work/transaction"
timeout 90s "$work/transaction" > "$work/transaction.gcc.txt"
golden_output "$pkg/examples/file_transaction.expected.txt" "$work/transaction.gcc.txt"
for source in file_transaction.xl file_transaction_c.c; do
    timeout 90s ./release/xl run "$pkg/examples/$source" > "$work/$source.jit.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.jit.txt"
    timeout 180s ./release/xl build "$pkg/examples/$source" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.aot.txt"
done
printf '[gzip] runtime/ABI and thirteen ordinary AOT paths: PASS\n'
failed=0
status=0
timeout 90s "$work/file_rollback_c" --closed > "$work/closed.txt" 2>&1 || status=$?
printf 'closed_rollback\t%s\n' "$status" > "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/closed.txt" >&2; failed=1; fi
status=0
timeout 240s bash "$root/tools/check_extlib_native_globals.sh" gzip > "$work/lifecycle.txt" 2>&1 || status=$?
printf 'top_level_lifecycle\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/lifecycle.txt" >&2; failed=1; fi
test "$failed" -eq 0
