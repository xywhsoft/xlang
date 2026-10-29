#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-snappy-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[snappy] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/snappy/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

for order in ALONE CORE_FIRST NATIVE_FIRST XRT_FIRST; do
    "$cc" "${flags[@]}" "-DSNAPPY_$order" -fsyntax-only release/extlibs/snappy/test/native_header.c
done

for kind in compressor decompressor; do
    case="private_${kind}_negative"
    if timeout 90s ./release/xl check "release/extlibs/snappy/test/$case.xl" > "$work/$case.txt" 2>&1; then
        printf 'snappy: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq 'updateNative' "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'snappy: expected private-access diagnostic\n' >&2
        exit 1
    fi
done

"$cc" "${flags[@]}" -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/snappy/xrt_snappy.c -o "$work/core.o"
"$cc" "${flags[@]}" -c release/extlibs/snappy/snappy_native.c -o "$work/bridge.o"
"$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" \
    -c release/extlibs/snappy/third_party/xrt_snappy.c -o "$work/vendor.o"
objects=("$work/runtime.o" "$work/core.o" "$work/bridge.o" "$work/vendor.o")
"$cc" "${flags[@]}" -DNDEBUG -DSNAPPY_MEMORY_GUARD \
    release/extlibs/snappy/test/test_snappy.c \
    "${objects[@]}" -lm -ldl -o "$work/test_snappy"
"$cc" "${flags[@]}" release/extlibs/snappy/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
"$cc" "${flags[@]}" release/extlibs/snappy/examples/basic.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 90s "$work/test_snappy"
timeout 30s "$work/test_allocations"
timeout 30s "$work/basic" > "$work/basic_c.txt"
timeout 90s ./release/xl run release/extlibs/snappy/examples/basic.c > "$work/basic_direct.txt"
diff -u <(printf 'hello snappy\n') "$work/basic_c.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_snappy.xl
timeout 90s ./release/xl run test/cases/extlib_snappy_progress.xl
timeout 90s ./release/xl run release/extlibs/snappy/test/file_ops.xl > "$work/file_ops_jit.txt"
timeout 90s ./release/xl run release/extlibs/snappy/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/snappy/test/text_transfer.xl
timeout 90s ./release/xl run release/examples/extlibs/snappy.xl > "$work/full_xl.txt"
timeout 90s ./release/xl run release/examples/extlibs/snappy_basic.xl > "$work/basic_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
printf '[snappy] XLang AOT gate\n'
for name in central progress file_ops allocations text_transfer basic full; do
    printf '[snappy] AOT %s\n' "$name"
    case "$name" in
        central) source=test/cases/extlib_snappy.xl ;;
        progress) source=test/cases/extlib_snappy_progress.xl ;;
        basic) source=release/examples/extlibs/snappy_basic.xl ;;
        full) source=release/examples/extlibs/snappy.xl ;;
        *) source="release/extlibs/snappy/test/$name.xl" ;;
    esac
    timeout 180s ./release/xl build "$source" -o "$work/snappy_$name"
    timeout 90s "$work/snappy_$name" > "$work/${name}_aot.txt"
done
diff -u "$work/file_ops_jit.txt" "$work/file_ops_aot.txt"
diff -u "$work/basic_c.txt" "$work/basic_aot.txt"
diff -u "$work/full_xl.txt" "$work/full_aot.txt"
bash "$root/tools/check_extlib_native_globals.sh" snappy
printf 'snappy Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
