#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-blake3-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
pkg=release/extlibs/blake3
sources=("$pkg/blake3_portable.c" "$pkg/xrt_blake3.c" "$pkg/blake3_native.c")
input_paths() {
    find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \
        -o -name '*.md' -o -name '*.html' -o -name LICENSE \) -print0
    printf '%s\0' .gitattributes lib/xrt/xrt.h lib/xruntime/xruntime.h \
        test/support/extlib_codec_runtime.c tools/check_extlib_blake3.sh \
        release/xl test/cases/extlib_blake3.xl \
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
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_THREAD -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for index in "${!sources[@]}"; do
    object="$work/native-$index.o"
    "$cc" "${flags[@]}" -c "${sources[$index]}" -o "$object"
    objects+=("$object")
done
for name in test_blake3 test_blake3_alloc test_blake3_native test_blake3_close; do
    selected=("${objects[@]}")
    # 核心测试包含 core，不重复链接实现；其余测试链接公开接口。
    if [[ "$name" == test_blake3 ]]; then selected=("${objects[0]}"); fi
    # 关闭故障用例在本编译单元包含薄绑定，避免重复链接。
    if [[ "$name" == test_blake3_close ]]; then selected=("${objects[0]}" "${objects[1]}"); fi
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
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
    if "$xl" check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'BLAKE3 native method escaped its class: %s\n' "$name" >&2
        exit 1
    fi
    grep -Fq 'no matching callable class member' "$work/$name.log"
done
if "$xl" check "$pkg/test/keyed_pointer_length_negative.xl" > "$work/keyed_pointer_length.log" 2>&1; then
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
# 重新枚举也覆盖新增、删除文件；哈希仅相同不代表输入集合未变化。
mapfile -d '' inputs_after < <(input_paths | sort -z)
sha256sum "${inputs_after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'BLAKE3 Linux C ASan/UBSan and XLang parity gate: PASS\n'
