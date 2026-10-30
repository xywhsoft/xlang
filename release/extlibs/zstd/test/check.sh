#!/usr/bin/env bash
set -euo pipefail

mode=${1:-}
if [[ -n $mode && $mode != --c-only ]]; then
    printf 'usage: %s [--c-only]\n' "$0" >&2
    exit 2
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zstd-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if [[ -f $work/inputs.before.sha256 ]]; then
        if ! sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"; then result=1; fi
    fi
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[zstd] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
cc=${CC:-gcc}
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/zstd/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(
    { find release/extlibs/zstd release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_codec_runtime.c test/cases/extlib_zstd.xl test/cases/extlib_zstd_progress.xl \
        release/examples/extlibs/zstd.xl tools/check_extlib_native_globals.sh tools/check_extlib_native_globals.ps1
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"

if [[ $mode != --c-only ]]; then
    for kind in compressor decompressor; do
        case="private_${kind}_negative"
        if timeout 90s ./release/xl check "release/extlibs/zstd/test/$case.xl" > "$work/$case.txt" 2>&1; then
            printf 'zstd: %s must be rejected\n' "$case" >&2
            exit 1
        fi
        if ! grep -Fq 'updateNative' "$work/$case.txt" ||
           ! grep -Fq 'access' "$work/$case.txt"; then
            cat "$work/$case.txt" >&2
            printf 'zstd: expected private-access diagnostic\n' >&2
            exit 1
        fi
    done
fi

timeout 120s "$cc" "${flags[@]}" -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
timeout 120s "$cc" "${flags[@]}" -c release/extlibs/zstd/xrt_zstd.c -o "$work/core.o"
timeout 120s "$cc" "${flags[@]}" -c release/extlibs/zstd/zstd_native.c -o "$work/bridge.o"
timeout 180s "$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" \
    -c release/extlibs/zstd/third_party/zstd.c -o "$work/vendor.o"
objects=("$work/runtime.o" "$work/core.o" "$work/bridge.o" "$work/vendor.o")
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_zstd.c \
    "${objects[@]}" -lm -ldl -o "$work/test_zstd"
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_file.c \
    "${objects[@]}" -lm -ldl -o "$work/test_file"
"$cc" "${flags[@]}" release/extlibs/zstd/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
"$cc" "${flags[@]}" release/extlibs/zstd/examples/basic_c.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 120s "$cc" "${flags[@]}" release/extlibs/zstd/test/test_backend_oom.c \
    "$work/runtime.o" "$work/vendor.o" -lm -ldl -o "$work/backend_oom"
timeout 90s "$work/backend_oom"
timeout 120s "$cc" "${flags[@]}" release/extlibs/zstd/examples/bounded_stream_c.c \
    "${objects[@]}" -lm -ldl -o "$work/bounded_c"
printf 'source bytes: 7\ncompleted frames: 1\nbinary round trip: true\noutput limit rejected: true\nzstd bounded stream: OK\n' > "$work/bounded.golden"
timeout 90s "$work/bounded_c" > "$work/bounded_c.txt"
diff -u "$work/bounded.golden" "$work/bounded_c.txt"
timeout 90s "$work/test_zstd"
timeout 30s "$work/test_file"
timeout 30s "$work/test_allocations"
timeout 30s "$work/basic" > "$work/basic_c.txt"
if [[ $mode == --c-only ]]; then
    printf 'zstd Linux C ASan/UBSan gate: PASS\n'
    exit 0
fi
for source in test/test_backend_oom.c examples/bounded_stream.xl examples/bounded_stream_c.c; do
    name=$(basename "$source")
    timeout 90s release/xl run "release/extlibs/zstd/$source" > "$work/$name.jit.txt"
    timeout 180s release/xl build "release/extlibs/zstd/$source" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot" > "$work/$name.aot.txt"
    diff -u "$work/$name.jit.txt" "$work/$name.aot.txt"
    if [[ $name != test_backend_oom.c ]]; then diff -u "$work/bounded.golden" "$work/$name.jit.txt"; fi
done
timeout 90s ./release/xl run release/extlibs/zstd/examples/basic_c.c > "$work/basic_direct.txt"
diff -u "$work/basic_c.txt" "$work/basic_direct.txt"
timeout 90s ./release/xl run test/cases/extlib_zstd.xl
timeout 90s ./release/xl run test/cases/extlib_zstd_progress.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/wire_fixture.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/file_ops.xl > "$work/file_ops_jit.txt"
timeout 90s ./release/xl run release/extlibs/zstd/test/concat_boundary.xl
timeout 90s ./release/xl run release/extlibs/zstd/test/error_dispatch.xl
timeout 45s ./release/xl run release/extlibs/zstd/examples/basic.xl > "$work/basic_xl.txt"
diff -u "$work/basic_c.txt" "$work/basic_xl.txt"
timeout 90s ./release/xl run release/examples/extlibs/zstd.xl > "$work/teaching.txt"
grep -Fq 'zstd example: PASS' "$work/teaching.txt"
printf '[zstd] XLang AOT gate\n'
for name in central progress wire_fixture concat_boundary error_dispatch file_ops; do
    printf '[zstd] AOT %s\n' "$name"
    case "$name" in
        central) source=test/cases/extlib_zstd.xl ;;
        progress) source=test/cases/extlib_zstd_progress.xl ;;
        *) source="release/extlibs/zstd/test/$name.xl" ;;
    esac
    timeout 180s ./release/xl build "$source" -o "$work/zstd_$name"
    timeout 90s "$work/zstd_$name" > "$work/${name}_aot.txt"
done
diff -u "$work/file_ops_jit.txt" "$work/file_ops_aot.txt"
for name in basic teaching; do
    printf '[zstd] AOT example %s\n' "$name"
    source=release/extlibs/zstd/examples/basic.xl
    if [[ $name == teaching ]]; then source=release/examples/extlibs/zstd.xl; fi
    timeout 180s ./release/xl build "$source" -o "$work/zstd_$name"
    timeout 90s "$work/zstd_$name" > "$work/${name}_aot.txt"
done
diff -u "$work/basic_c.txt" "$work/basic_aot.txt"
diff -u "$work/teaching.txt" "$work/teaching_aot.txt"
bash "$root/tools/check_extlib_native_globals.sh" zstd
printf 'zstd Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
