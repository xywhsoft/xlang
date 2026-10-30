#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-blake3-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[blake3] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
pkg=release/extlibs/blake3
sources=("$pkg/blake3_portable.c" "$pkg/xrt_blake3.c" "$pkg/blake3_native.c")
input_paths() {
    find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \
        -o -name '*.md' -o -name '*.html' -o -name LICENSE \) -print0
    printf '%s\0' .gitattributes lib/xrt/xrt.h lib/xruntime/xruntime.h \
        test/support/extlib_codec_runtime.c tools/check_extlib_blake3.sh \
        release/xl release/libxl.so release/libxlc.so tools/check_extlib_blake3.ps1 \
        test/cases/extlib_blake3.xl \
        test/cases/extlib_blake3_boundaries.xl test/cases/extlib_blake3_progress.xl \
        release/examples/extlibs/blake3.xl release/examples/extlibs/blake3_basic.xl \
        release/examples/extlibs/blake3_basic.expected.txt
}
mapfile -d '' inputs < <(input_paths | sort -z)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"
timeout 120s "$cc" "${flags[@]}" -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_THREAD -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for index in "${!sources[@]}"; do
    object="$work/native-$index.o"
    timeout 120s "$cc" "${flags[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
done
for name in test_blake3 test_blake3_alloc test_blake3_native test_blake3_close test_reader_cause; do
    selected=("${objects[@]}")
    # 核心测试包含 core，不重复链接实现；其余测试链接公开接口。
    if [[ "$name" == test_blake3 ]]; then selected=("${objects[0]}"); fi
    # 关闭故障用例在本编译单元包含薄绑定，避免重复链接。
    if [[ "$name" == test_blake3_close ]]; then selected=("${objects[0]}" "${objects[1]}"); fi
    if [[ "$name" == test_reader_cause ]]; then selected=(); fi
    timeout 120s "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
timeout 120s "$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/blake3_basic.expected.txt "$work/basic.log"
xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'BLAKE3 Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u release/examples/extlibs/blake3_basic.expected.txt "$work/basic_direct.log"
for name in native_update_visibility_negative native_keyed_update_visibility_negative; do
    if timeout 90s "$xl" check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'BLAKE3 native method escaped its class: %s\n' "$name" >&2
        exit 1
    fi
    grep -Fq 'no matching callable class member' "$work/$name.log"
done
if timeout 90s "$xl" check "$pkg/test/keyed_pointer_length_negative.xl" > "$work/keyed_pointer_length.log" 2>&1; then
    printf 'BLAKE3 raw keyed constructor remains callable\n' >&2
    exit 1
fi
grep -Eq '\[sem\].*create' "$work/keyed_pointer_length.log"
for name in extlib_blake3 extlib_blake3_boundaries extlib_blake3_progress; do
    timeout 90s "$xl" run "test/cases/$name.xl"
done
timeout 90s "$xl" run "$pkg/test/vectors.xl"
timeout 90s "$xl" run release/examples/extlibs/blake3.xl > "$work/full_xl.log"
timeout 90s "$xl" run release/examples/extlibs/blake3_basic.xl > "$work/basic_xl.log"
diff -u release/examples/extlibs/blake3_basic.expected.txt "$work/basic_xl.log"
diff -u "$work/basic.log" "$work/basic_xl.log"
# 普通独立产物必须真正运行；不以原 JIT 和 C sanitizer 替代 AOT。
for source in test/cases/extlib_blake3.xl test/cases/extlib_blake3_boundaries.xl \
    test/cases/extlib_blake3_progress.xl "$pkg/test/vectors.xl" "$pkg/test/test_reader_cause.c" \
    "$pkg/examples/basic.c" release/examples/extlibs/blake3.xl release/examples/extlibs/blake3_basic.xl; do
    name=$(basename "$source")
    timeout 90s "$xl" run "$source" > "$work/$name.jit.txt"
    timeout 180s "$xl" build "$source" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot" > "$work/$name.aot.txt"
    diff -u "$work/$name.jit.txt" "$work/$name.aot.txt"
done
# 重新枚举也覆盖新增、删除文件；哈希仅相同不代表输入集合未变化。
mapfile -d '' inputs_after < <(input_paths | sort -z)
sha256sum "${inputs_after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
# 两个已知发布阻断都保留，负例只编译，绝不运行伪造句柄。
blocked=0
private_status=0
timeout 90s "$xl" check "$pkg/test/private_adopt_constructor_negative.xl" > "$work/private.log" 2>&1 || private_status=$?
printf 'private-adopt check exit=%s\n' "$private_status"
if ((private_status == 0)); then
    printf 'BLAKE3 private handle adoption remains accessible\n' >&2
    blocked=1
elif ((private_status != 1)) || ! grep -q '\[sem\]' "$work/private.log"; then
    cat "$work/private.log" >&2
    exit 1
fi
timeout 90s "$xl" run "$pkg/test/aot_global_hasher_repro.xl" > "$work/global.jit.txt"
timeout 180s "$xl" build "$pkg/test/aot_global_hasher_repro.xl" -o "$work/global.aot"
global_status=0
timeout 90s "$work/global.aot" > "$work/global.aot.txt" 2> "$work/global.stderr" || global_status=$?
printf 'global-hasher AOT exit=%s\n' "$global_status"
if ((global_status != 0)); then
    cat "$work/global.aot.txt" "$work/global.stderr" >&2
    blocked=1
else diff -u "$work/global.jit.txt" "$work/global.aot.txt"; fi
if ((blocked)); then printf 'BLAKE3 release blockers remain; artifacts=%s\n' "$work" >&2; exit 1; fi
printf 'BLAKE3 Linux C ASan/UBSan, ordinary AOT and XLang parity gate: PASS\n'
