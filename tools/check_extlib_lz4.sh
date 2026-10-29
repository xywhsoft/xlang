#!/usr/bin/env bash
set -euo pipefail

# C sanitizer 与已存在的 XLang 边界、文件进度和双语对照。
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
work=${LZ4_TEST_WORK:-"$root/build/test/lz4_linux_guard"}
mkdir -p "$work"
cc=${CC:-gcc}
pkg=release/extlibs/lz4
input_paths() {
    find "$pkg" -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h test/support/extlib_codec_runtime.c \
        tools/check_extlib_lz4.sh tools/check_extlib_lz4_interop.py release/xl \
        test/cases/extlib_lz4.xl test/cases/extlib_lz4_progress.xl \
        release/examples/extlibs/lz4.xl release/examples/extlibs/lz4_basic.xl \
        release/examples/extlibs/lz4_files.xl \
        release/examples/extlibs/lz4_basic.expected.txt release/examples/extlibs/lz4_files.expected.txt
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Ilib/xruntime "${sanitize[@]}")
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
    if [[ $result == 0 ]]; then printf 'LZ4 Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'; fi
    exit "$result"
}
trap finish_gate EXIT
for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "$pkg/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'lz4: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'lz4: expected private-access diagnostic\n' >&2
        exit 1
    fi
done
"$cc" --version > "$work/compiler.txt"
"$cc" "${flags[@]}" -DXRT_MODULE_DIR -DXRT_MODULE_DIR_TEMP -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_PATH \
    -DXRT_MODULE_THREAD -DXRT_MODULE_ATOMIC \
    -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
for source in xrt_lz4 xrt_lz4_file lz4_native; do
    "$cc" "${flags[@]}" -c "$pkg/$source.c" -o "$work/$source.o"
    objects+=("$work/$source.o")
done
for source in lz4_frame lz4_hash third_party/lz4 third_party/lz4hc; do
    object="$work/vendor-$(basename "$source").o"
    "$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" -c "$pkg/$source.c" -o "$object"
    objects+=("$object")
done
for name in boundaries stream alloc threads native files; do
    "$cc" "${flags[@]}" -DNDEBUG "$pkg/test/test_lz4_$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    args=()
    if [[ $name == files || $name == native ]]; then args=("$work/$name-$$"); fi
    timeout 90s "$work/$name" "${args[@]}" 2>&1 | tee "$work/$name.log"
done
"$cc" "${flags[@]}" "$pkg/test/lz4_interop.c" "${objects[@]}" -lm -ldl -o "$work/interop"
python3 tools/check_extlib_lz4_interop.py "$work/interop" "$work/interop-files" \
    --library /usr/lib/x86_64-linux-gnu/liblz4.so.1 | tee "$work/interop.log"
for name in basic files; do
    "$cc" "${flags[@]}" "$pkg/examples/$name.c" "${objects[@]}" -lm -ldl -o "$work/example-$name"
    timeout 30s "$work/example-$name" > "$work/example-$name.log"
    diff -u "release/examples/extlibs/lz4_$name.expected.txt" "$work/example-$name.log"
done
timeout 90s ./release/xl run "$pkg/examples/basic.c" > "$work/example-basic-single-file.log"
timeout 90s ./release/xl run "$pkg/examples/files.c" > "$work/example-files-single-file.log"
diff -u "$work/example-files.log" "$work/example-files-single-file.log"
diff -u "$work/example-basic.log" "$work/example-basic-single-file.log"
timeout 90s ./release/xl run "$pkg/test/boundaries.xl"
timeout 90s ./release/xl run test/cases/extlib_lz4_progress.xl
for name in basic files; do
    timeout 90s ./release/xl run "release/examples/extlibs/lz4_$name.xl" > "$work/example-xl-$name.log"
    diff -u "release/examples/extlibs/lz4_$name.expected.txt" "$work/example-xl-$name.log"
    diff -u "$work/example-$name.log" "$work/example-xl-$name.log"
done
printf '[lz4] XLang JIT/AOT gate\n'
timeout 90s ./release/xl run test/cases/extlib_lz4.xl
timeout 90s ./release/xl run release/examples/extlibs/lz4.xl > "$work/example-xl-full.log"
for name in central boundaries progress; do
    printf '[lz4] AOT %s\n' "$name"
    case "$name" in
        central) source=test/cases/extlib_lz4.xl ;;
        boundaries) source="$pkg/test/boundaries.xl" ;;
        progress) source=test/cases/extlib_lz4_progress.xl ;;
    esac
    timeout 180s ./release/xl build "$source" -o "$work/lz4-$name-aot"
    timeout 90s "$work/lz4-$name-aot"
done
for name in full basic files; do
    printf '[lz4] AOT example %s\n' "$name"
    source=release/examples/extlibs/lz4.xl
    if [[ $name != full ]]; then source="release/examples/extlibs/lz4_$name.xl"; fi
    timeout 180s ./release/xl build "$source" -o "$work/lz4-$name-aot"
    timeout 90s "$work/lz4-$name-aot" > "$work/example-xl-$name-aot.log"
    diff -u "$work/example-xl-$name.log" "$work/example-xl-$name-aot.log"
    if [[ $name != full ]]; then
        diff -u "$work/example-$name.log" "$work/example-xl-$name-aot.log"
    fi
done
