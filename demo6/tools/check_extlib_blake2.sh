#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-blake2-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
pkg=release/extlibs/blake2
sources=("$pkg/third_party/blake2.c" "$pkg/xrt_blake2.c" "$pkg/blake2_native.c")
mapfile -d '' inputs < <(find "$pkg" -type f \( -name '*.c' -o -name '*.h' -o -name '*.json' -o -name '*.xl' \) -print0 | sort -z)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c
         tools/check_extlib_blake2.sh release/xl
         test/cases/extlib_blake2.xl test/cases/extlib_blake2_boundaries.xl
         test/cases/extlib_blake2_progress.xl
         release/examples/extlibs/blake2.xl release/examples/extlibs/blake2_basic.xl
         release/examples/extlibs/blake2_basic.expected.txt)
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
for name in test_blake2 test_blake2_alloc test_blake2_native test_blake2_close; do
    selected=("${objects[@]}")
    # 关闭故障用例在本编译单元包含薄绑定，避免重复链接。
    if [[ "$name" == test_blake2_close ]]; then selected=("${objects[0]}" "${objects[1]}"); fi
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/basic.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/basic"
timeout 30s "$work/basic" > "$work/basic.log"
diff -u release/examples/extlibs/blake2_basic.expected.txt "$work/basic.log"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'BLAKE2 Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run "$pkg/examples/basic.c" > "$work/basic_direct.log"
diff -u release/examples/extlibs/blake2_basic.expected.txt "$work/basic_direct.log"
for name in native_b_visibility_negative native_b_keyed_visibility_negative \
    native_s_visibility_negative native_s_keyed_visibility_negative; do
    if "$xl" check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'BLAKE2 native method escaped its class: %s\n' "$name" >&2
        exit 1
    fi
    grep -Fq 'no matching callable class member' "$work/$name.log"
done
for name in keyed_b_pointer_length_negative keyed_s_pointer_length_negative; do
    if "$xl" check "$pkg/test/$name.xl" > "$work/$name.log" 2>&1; then
        printf 'BLAKE2 raw keyed constructor remains callable: %s\n' "$name" >&2
        exit 1
    fi
    grep -Eq '\[sem\].*create' "$work/$name.log"
done
for name in extlib_blake2 extlib_blake2_boundaries extlib_blake2_progress; do
    timeout 90s "$xl" run "test/cases/$name.xl"
done
timeout 90s "$xl" run "$pkg/test/vectors.xl"
timeout 90s "$xl" run release/examples/extlibs/blake2.xl > "$work/full_xl.log"
timeout 90s "$xl" run release/examples/extlibs/blake2_basic.xl > "$work/basic_xl.log"
diff -u release/examples/extlibs/blake2_basic.expected.txt "$work/basic_xl.log"
diff -u "$work/basic.log" "$work/basic_xl.log"
sha256sum "${inputs[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'BLAKE2 Linux C ASan/UBSan and XLang parity gate: PASS\n'
