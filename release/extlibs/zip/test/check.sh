#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zip-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
cc1_path=''
python_path=$(command -v python3)
input_paths() {
    find release/extlibs/zip release/extlibs/_libzip release/extlibs/_zlib release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        release/examples/extlibs/zip.xl release/examples/extlibs/zip_c.c "$python_path"
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$cc1_path" ]]; then printf '%s\0' "$cc1_path"; fi
}
fingerprint() {
    mapfile -d '' inputs < <(input_paths | sort -zu)
    sha256sum "${inputs[@]}"
}
cleanup() {
    result=$?
    trap - EXIT ERR
    set +e
    cd "$root" || exit 125
    fingerprint > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[zip] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
fingerprint > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
cc1_path=$("$cc" -print-prog-name=cc1)
fingerprint > "$work/inputs.before.sha256"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
flags=(-std=c11 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -O1 -g -Wall -Wextra
    -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
    -Ilib/xrt -Irelease/extlibs/zip/include -Irelease/extlibs/zip
    -Irelease/extlibs/_libzip/third_party/libzip/lib -Irelease/extlibs/_zlib/third_party/zlib)
links=(-no-pie -lm -ldl -pthread)

# 从发布清单递归读取真实编译单元；不维护第二套生产源列表。
python3 - <<'PY' > "$work/sources"
import json
from pathlib import Path
seen = set()
def visit(name):
    if name in seen:
        return
    seen.add(name)
    folder = Path('release/extlibs') / name
    manifest = json.loads((folder / 'xlang.package.json').read_text(encoding='utf-8'))
    for dependency in manifest.get('dependencies', []):
        visit(dependency['name'])
    for source in manifest['artifact'].get('cSources', []):
        path = folder / source
        if not path.is_file():
            raise ValueError(f'missing ZIP C source: {path}')
        print(path)
visit('zip')
PY
mapfile -t sources < "$work/sources"
cargs=("${sources[@]}")
for dir in lib/xrt release/extlibs/zip/include release/extlibs/zip \
    release/extlibs/_libzip/third_party/libzip/lib release/extlibs/_zlib/third_party/zlib; do
    cargs+=(--inc-path "$dir")
done
timeout 60s "$cc" "${flags[@]}" -Werror -fsyntax-only -DXRT_MODULE_FILE_TEMP \
    release/extlibs/_libzip/platform/zip_source_file_xrt.c
timeout 60s "$cc" "${flags[@]}" -Werror -fsyntax-only \
    release/extlibs/zip/test/test_commit_memory.c release/extlibs/zip/examples/memory_roundtrip_c.c
timeout 240s "$cc" "${flags[@]}" -c release/extlibs/zip/test/xrt_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
index=0
for source in "${sources[@]}"; do
    object="$work/source_$index.o"
    if [[ "$source" == release/extlibs/zip/* ]]; then
        timeout 60s "$cc" "${flags[@]}" -Werror -fsyntax-only "$source"
    fi
    timeout 90s "$cc" "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
    index=$((index + 1))
done
for name in test_zip test_commit_memory; do
    timeout 90s "$cc" "${flags[@]}" -Werror "release/extlibs/zip/test/$name.c" \
        "${objects[@]}" "${links[@]}" -o "$work/$name"
    timeout 120s "$work/$name"
done
timeout 90s "$cc" "${flags[@]}" -Werror release/extlibs/zip/test/interop.c \
    "${objects[@]}" "${links[@]}" -o "$work/interop"
XL_ZIP_INTEROP_RUNNER="$work/interop" timeout 120s python3 -I release/extlibs/zip/test/interop.py
timeout 90s "$cc" "${flags[@]}" -Werror release/examples/extlibs/zip_c.c \
    "${objects[@]}" "${links[@]}" -o "$work/original_c"
timeout 30s "$work/original_c" > "$work/original.txt"
diff -u <(printf 'zip C/XL example: PASS\n') "$work/original.txt"
timeout 90s "$cc" "${flags[@]}" -Werror release/extlibs/zip/examples/memory_roundtrip_c.c \
    "${objects[@]}" "${links[@]}" -o "$work/memory_gcc"
timeout 30s "$work/memory_gcc" "$work/gcc.zip" > "$work/memory.txt"
diff -u <(sed 's/\r$//' release/extlibs/zip/examples/memory_roundtrip.expected.txt) "$work/memory.txt"
python3 -I release/extlibs/zip/test/check_memory_wire.py "$work/gcc.zip"

# WSL 挂载盘的逐文件访问很慢；JIT/AOT 始终用本次独占的原生临时副本。
stage="$work/stage"
mkdir -p "$stage/release/extlibs" "$stage/release/examples/extlibs" "$stage/lib/xrt" "$stage/lib/xruntime"
cp -a release/xl release/libxl.so release/libxlc.so release/include "$stage/release/"
cp -a release/extlibs/zip release/extlibs/_libzip release/extlibs/_zlib "$stage/release/extlibs/"
cp -a lib/xrt/xrt.h "$stage/lib/xrt/"
cp -a lib/xruntime/xruntime.h "$stage/lib/xruntime/"
cp -a release/examples/extlibs/zip.xl release/examples/extlibs/zip_c.c "$stage/release/examples/extlibs/"
run_xl() { ( cd "$stage" && timeout 240s ./release/xl "$@" ); }
reject() {
    local name=$1 source=$2 symbol=$3 status=0
    run_xl check "$source" > "$work/$name.txt" 2>&1 || status=$?
    if ((status != 1)); then cat "$work/$name.txt" >&2; echo "$name: expected 1, got $status" >&2; return 1; fi
    grep -Eq 'access|private' "$work/$name.txt"
    if [[ -n "$symbol" ]]; then grep -Fq "$symbol" "$work/$name.txt"; fi
}
run_xl run release/extlibs/zip/test/test_commit_memory.c "${cargs[@]}"
run_xl run release/extlibs/zip/test/commit_memory.xl > "$work/commit.txt"
diff -u <(printf 'zip commit MemoryError/retry: PASS\n') "$work/commit.txt"
run_xl run release/extlibs/zip/test/text_transfer.xl > "$work/text.txt"
test ! -s "$work/text.txt"
run_xl run release/extlibs/zip/test/boundaries.xl > "$work/boundaries.txt"
diff -u <(printf 'zip boundaries: ok\n') "$work/boundaries.txt"
reject private_native release/extlibs/zip/test/private_native_negative.xl addNative

run_xl cgen release/extlibs/zip/test/boundaries.xl --emit "$work/generated.c"
python3 - "$work/generated.c" "$work" <<'PY'
import re
import sys
from pathlib import Path
generated, work = Path(sys.argv[1]), Path(sys.argv[2])
decls = re.findall(r'^extern [^\r\n;]*\bxzip_[a-z_]+\([^;\r\n]*\);', generated.read_text(), re.M)
expected = set(re.findall(r'extern "(xzip_[a-z_]+)"', Path('release/extlibs/zip/src/zip.xl').read_text()))
actual = {re.search(r'\bxzip_[a-z_]+(?=\()', declaration)[0] for declaration in decls}
if len(decls) != 32 or expected != actual:
    raise ValueError('ZIP generated ABI coverage incomplete')
body = '#define XRT_MODULE_ALL\n' + ''.join(f'#include "{source}"\n' for source in
    ('xrt_zip_source.c', 'xrt_zip_archive.c', 'xrt_zip_extract.c', 'zip_native.c')) + '\n'.join(decls)
(work / 'abi.c').write_text(body)
wrong = re.sub(r'^extern [^\r\n;]*\bxzip_text_size\(', 'extern int xzip_text_size(', body, flags=re.M)
if wrong == body:
    raise ValueError('ABI negative did not mutate declaration')
(work / 'abi_wrong.c').write_text(wrong)
PY
timeout 60s "$cc" "${flags[@]}" -Werror -fsyntax-only "$work/abi.c"
status=0
timeout 60s "$cc" "${flags[@]}" -Werror -fsyntax-only "$work/abi_wrong.c" > "$work/abi_wrong.txt" 2>&1 || status=$?
test "$status" -eq 1
grep -q 'conflicting types' "$work/abi_wrong.txt"

for kind in c xl; do
    if [[ "$kind" == c ]]; then
        source=release/extlibs/zip/examples/memory_roundtrip_c.c
        run_xl run "$source" "${cargs[@]}" -- "$work/cjit.zip" > "$work/cjit.txt"
    else
        source=release/extlibs/zip/examples/memory_roundtrip.xl
        run_xl run "$source" -- "$work/xljit.zip" > "$work/xljit.txt"
    fi
    diff -u "$work/memory.txt" "$work/${kind}jit.txt"
    python3 -I release/extlibs/zip/test/check_memory_wire.py "$work/${kind}jit.zip"
done
run_xl run release/examples/extlibs/zip.xl > "$work/original_xl.txt"
diff -u "$work/original.txt" "$work/original_xl.txt"

for name in aot_core text_transfer boundaries commit_memory; do
    run_xl build "release/extlibs/zip/test/$name.xl" -o "$work/$name"
    timeout 60s "$work/$name" > "$work/$name.aot.txt"
    case "$name" in
        aot_core|text_transfer) test ! -s "$work/$name.aot.txt" ;;
        boundaries) diff -u "$work/boundaries.txt" "$work/$name.aot.txt" ;;
        commit_memory) diff -u "$work/commit.txt" "$work/$name.aot.txt" ;;
    esac
done
run_xl build release/examples/extlibs/zip.xl -o "$work/original_xl"
timeout 60s "$work/original_xl" > "$work/original_xl.aot.txt"
diff -u "$work/original.txt" "$work/original_xl.aot.txt"
run_xl build release/extlibs/zip/test/c_aot_import.xl release/examples/extlibs/zip_c.c -o "$work/original_c_aot"
timeout 60s "$work/original_c_aot" > "$work/original_c.aot.txt"
diff -u "$work/original.txt" "$work/original_c.aot.txt"
run_xl build release/extlibs/zip/test/test_commit_memory.c "${cargs[@]}" -o "$work/commit_c"
timeout 60s "$work/commit_c"
for kind in c xl; do
    arguments=()
    source=release/extlibs/zip/examples/memory_roundtrip.xl
    if [[ "$kind" == c ]]; then source=release/extlibs/zip/examples/memory_roundtrip_c.c; arguments=("${cargs[@]}"); fi
    run_xl build "$source" "${arguments[@]}" -o "$work/memory_$kind"
    timeout 60s "$work/memory_$kind" "$work/$kind.aot.zip" > "$work/$kind.aot.txt"
    diff -u "$work/memory.txt" "$work/$kind.aot.txt"
    python3 -I release/extlibs/zip/test/check_memory_wire.py "$work/$kind.aot.zip"
done
printf '[zip] runtime/ABI/interop and nine ordinary AOT paths: PASS\n'
# 后续独立边界收集完整原始退出码，不把主线失败改成预期成功。
failed=0
status=0
XL_ZIP_RELEASE_DIR="$stage/release" timeout 240s bash release/extlibs/zip/test/check_symlink.sh \
    > "$work/symlink.txt" 2>&1 || status=$?
printf 'symlink\t%s\n' "$status" > "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/symlink.txt" >&2; failed=1; fi
run_xl run release/extlibs/zip/test/global_lifecycle.xl > "$work/global.jit.txt"
diff -u <(printf 'zip globals: PASS\n') "$work/global.jit.txt"
run_xl build release/extlibs/zip/test/global_lifecycle.xl -o "$work/global"
status=0
timeout 60s "$work/global" > "$work/global.aot.txt" 2>&1 || status=$?
printf 'global_aot\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/global.aot.txt" >&2; failed=1
else diff -u "$work/global.jit.txt" "$work/global.aot.txt"; fi
status=0
run_xl run release/extlibs/zip/test/error_messages.xl > "$work/error_messages.jit.txt" 2>&1 || status=$?
printf 'error_messages_jit\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/error_messages.jit.txt" >&2; failed=1
else diff -u <(printf 'zip described errors: 2\n') "$work/error_messages.jit.txt"; fi
run_xl build release/extlibs/zip/test/error_messages.xl -o "$work/error_messages"
status=0
timeout 60s "$work/error_messages" > "$work/error_messages.aot.txt" 2>&1 || status=$?
printf 'error_messages_aot\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/error_messages.aot.txt" >&2; failed=1
else diff -u <(printf 'zip described errors: 2\n') "$work/error_messages.aot.txt"; fi
status=0
reject private_memory release/extlibs/zip/test/private_memory_constructor_negative.xl '' || status=$?
printf 'private_constructor_check\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then failed=1; fi
test "$failed" -eq 0
printf 'zip Linux complete gate: PASS\n'
