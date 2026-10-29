#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work="$root/build/test/murmur3_linux_guard"
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/murmur3
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' .gitattributes lib/xrt/xrt.h lib/xruntime/xruntime.h \
        test/support/extlib_codec_runtime.c tools/check_extlib_murmur3.sh \
        release/examples/extlibs/murmur3_basic.expected.txt release/examples/extlibs/murmur3.expected.txt
}
mapfile -d '' inputs < <(input_paths | sort -z)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_THREAD -DXRT_MODULE_ATOMIC -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_BUFFER -DXRT_MODULE_DIR -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for source in third_party/MurmurHash3.c xrt_murmur3.c murmur3_native.c; do
    object="$work/native-${#objects[@]}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
"$cc" "${flags[@]}" "$pkg/test/test_murmur3_standalone.c" "${objects[0]}" "${objects[1]}" -lm -ldl -o "$work/standalone"
timeout 30s "$work/standalone" > "$work/standalone.log"
for name in test_murmur3 test_murmur3_native; do
    selected=("${objects[@]}")
    if [[ "$name" == test_murmur3 ]]; then selected=(); fi
    "$cc" "${flags[@]}" "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
for name in basic files; do
    stem=murmur3_basic
    if [[ "$name" == files ]]; then stem=murmur3; fi
    "$cc" "${flags[@]}" "$pkg/examples/$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.log"
    diff -u "release/examples/extlibs/$stem.expected.txt" "$work/$name.log"
done
mapfile -d '' after < <(input_paths | sort -z)
sha256sum "${after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'murmur3 Linux C ASan/UBSan gate: PASS\n'
