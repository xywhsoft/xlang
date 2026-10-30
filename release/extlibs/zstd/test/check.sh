#!/usr/bin/env bash
set -Eeuo pipefail

mode=${1:-}
if [[ -n $mode && $mode != --c-only ]]; then
    printf 'usage: %s [--c-only]\n' "$0" >&2
    exit 2
fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zstd-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
cc1_path=''
input_paths() {
    find release/extlibs/zstd release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_codec_runtime.c test/support/extlib_codec_io_errors.h \
        test/support/extlib_native_codec_file_rollback.h test/cases/extlib_zstd.xl test/cases/extlib_zstd_progress.xl \
        release/examples/extlibs/zstd.xl tools/check_extlib_native_globals.sh tools/check_extlib_native_globals.ps1
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
    if [[ -f $work/inputs.before.sha256 ]]; then
        fingerprint > "$work/inputs.after.sha256"
        if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[zstd] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_DIR_TEMP -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/zstd/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fingerprint > "$work/inputs.before.sha256"
timeout 30s "$cc" --version > "$work/compiler.txt"
cc1_path=$("$cc" -print-prog-name=cc1)
fingerprint > "$work/inputs.before.sha256"
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }
compile -Wmissing-prototypes -fsyntax-only release/extlibs/zstd/zstd_native.c

if [[ $mode != --c-only ]]; then
    for kind in compressor decompressor; do
        case="private_${kind}_negative"
        status=0
        timeout 90s ./release/xl check "release/extlibs/zstd/test/$case.xl" > "$work/$case.txt" 2>&1 || status=$?
        if ((status != 1)); then
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
compile release/extlibs/zstd/test/test_zstd.c \
    "${objects[@]}" -lm -ldl -o "$work/test_zstd"
compile release/extlibs/zstd/test/test_file.c \
    "${objects[@]}" -lm -ldl -o "$work/test_file"
compile release/extlibs/zstd/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
compile release/extlibs/zstd/examples/basic_c.c \
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
for name in io_errors_c file_rollback_c text_views_c; do
    compile "release/extlibs/zstd/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
timeout 30s "$work/basic" > "$work/basic_c.txt"
if [[ $mode == --c-only ]]; then
    printf 'zstd Linux C ASan/UBSan gate: PASS\n'
    exit 0
fi
cargs=(release/extlibs/zstd/xrt_zstd.c release/extlibs/zstd/zstd_native.c release/extlibs/zstd/third_party/zstd.c
    --inc-path lib/xrt --inc-path lib/xruntime)
for name in io_errors_c file_rollback_c text_views_c; do
    extra=()
    if [[ $name != io_errors_c ]]; then extra=("${cargs[@]}"); fi
    timeout 90s ./release/xl run "release/extlibs/zstd/test/$name.c" "${extra[@]}"
    timeout 180s ./release/xl build "release/extlibs/zstd/test/$name.c" "${extra[@]}" -o "$work/$name.aot"
    timeout 90s "$work/$name.aot"
done
compile release/extlibs/zstd/examples/file_transaction_c.c "${objects[@]}" -lm -ldl -o "$work/transaction_c"
timeout 90s "$work/transaction_c" > "$work/transaction.gcc.txt"
diff -u release/extlibs/zstd/examples/file_transaction.expected.txt "$work/transaction.gcc.txt"
for source in file_transaction.xl file_transaction_c.c; do
    timeout 90s ./release/xl run "release/extlibs/zstd/examples/$source" > "$work/$source.jit.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.jit.txt"
    timeout 180s ./release/xl build "release/extlibs/zstd/examples/$source" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.aot.txt"
done
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
timeout 180s ./release/xl build release/extlibs/zstd/examples/basic_c.c -o "$work/basic_c.aot"
timeout 90s "$work/basic_c.aot" > "$work/basic_c.aot.txt"
diff -u "$work/basic_c.txt" "$work/basic_c.aot.txt"
printf '[zstd] ordinary runtime and seventeen AOT paths: PASS\n'
failed=0
status=0
timeout 90s "$work/file_rollback_c" --closed > "$work/closed.txt" 2>&1 || status=$?
printf 'closed_rollback\t%s\n' "$status" > "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/closed.txt" >&2; failed=1; fi
status=0
timeout 240s bash "$root/tools/check_extlib_native_globals.sh" zstd > "$work/lifecycle.txt" 2>&1 || status=$?
printf 'top_level_lifecycle\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/lifecycle.txt" >&2; failed=1; fi
test "$failed" -eq 0
printf 'zstd Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
