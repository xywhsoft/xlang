#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sha3-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
pkg=release/extlibs/sha3
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' .gitattributes lib/xrt/xrt.h lib/xruntime/xruntime.h \
        test/support/extlib_codec_runtime.c tools/check_extlib_sha3.sh \
        release/xl test/cases/extlib_sha3.xl test/cases/extlib_sha3_progress.xl \
        release/examples/extlibs/sha3.xl release/examples/extlibs/sha3_basic.xl \
        release/examples/extlibs/sha3_basic.expected.txt release/examples/extlibs/sha3.expected.txt
}
mapfile -d '' inputs < <(input_paths | sort -z)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_THREAD -DXRT_MODULE_ATOMIC -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_BUFFER -DXRT_MODULE_DIR_TEMP -DXRT_MODULE_PATH -DXRT_MODULE_FILE_WHOLE -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=()
for source in third_party/sha3.c xrt_sha3.c sha3_native.c; do
    object="$work/native-${#objects[@]}.o"
    "$cc" "${flags[@]}" -c "$pkg/$source" -o "$object"
    objects+=("$object")
done
"$cc" "${flags[@]}" "$pkg/test/test_sha3_standalone.c" "${objects[0]}" "${objects[1]}" -lm -ldl -o "$work/standalone"
timeout 30s "$work/standalone" > "$work/standalone.log"
for name in test_sha3 test_sha3_native; do
    selected=("${objects[@]}")
    if [[ "$name" == test_sha3 ]]; then selected=(); fi
    "$cc" "${flags[@]}" "$pkg/test/$name.c" "${selected[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 60s "$work/$name" 2>&1 | tee "$work/$name.log"
done
for name in basic files; do
    stem=sha3_basic
    if [[ "$name" == files ]]; then stem=sha3; fi
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/examples/$name.c" "${objects[@]}" "$work/runtime.o" -lm -ldl -o "$work/$name"
    timeout 30s "$work/$name" > "$work/$name.log"
    diff -u "release/examples/extlibs/$stem.expected.txt" "$work/$name.log"
done
for name in extlib_sha3 extlib_sha3_progress; do
    timeout 90s ./release/xl run "test/cases/$name.xl" > "$work/$name.log"
done
if ./release/xl check "$pkg/test/private_update_negative.xl" > "$work/private_update.log" 2>&1; then
    printf 'SHA3 native pointer/length method escaped its class\n' >&2
    exit 1
fi
grep -Fq 'no matching callable class member' "$work/private_update.log"
for name in sha3 sha3_basic; do
    timeout 90s ./release/xl run "release/examples/extlibs/$name.xl" > "$work/$name.log"
    diff -u "release/examples/extlibs/$name.expected.txt" "$work/$name.log"
done
timeout 90s ./release/xl run "$pkg/test/owned_text.xl" > "$work/owned_text.log"
grep -Fxq 'sha3 owned text ok' "$work/owned_text.log"
mapfile -d '' after < <(input_paths | sort -z)
sha256sum "${after[@]}" > "$work/inputs.after.sha256"
diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"
printf 'sha3 Linux C ASan/UBSan and XLang JIT gate: PASS\n'
