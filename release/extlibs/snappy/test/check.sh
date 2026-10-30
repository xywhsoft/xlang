#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-snappy-XXXXXX")
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
cc1_path=''
python_path=$(command -v python3 || true)
input_paths() {
    find release/extlibs/snappy release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/support/extlib_codec_runtime.c test/support/extlib_native_codec_file_rollback.h \
        test/cases/extlib_snappy.xl test/cases/extlib_snappy_progress.xl \
        release/examples/extlibs/snappy.xl release/examples/extlibs/snappy_basic.xl \
        tools/check_extlib_native_globals.ps1 tools/check_extlib_native_globals.sh
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$cc1_path" ]]; then printf '%s\0' "$cc1_path"; fi
    if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
}
fingerprint() {
    mapfile -d '' inputs < <(input_paths | sort -zu)
    sha256sum "${inputs[@]}"
}
cleanup() {
    result=$?
    trap - EXIT ERR
    set +e
    fingerprint > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[snappy] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
fingerprint > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
cc1_path=$("$cc" -print-prog-name=cc1)
fingerprint > "$work/inputs.before.sha256"
sanitize=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
flags=(-std=c11 -D_GNU_SOURCE -DXRT_MODULE_DIR_TEMP -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/extlibs/snappy/include "${sanitize[@]}")
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }

for order in ALONE CORE_FIRST NATIVE_FIRST XRT_FIRST; do
    compile "-DSNAPPY_$order" -fsyntax-only release/extlibs/snappy/test/native_header.c
done

for kind in compressor decompressor; do
    case="private_${kind}_negative"
    status=0
    timeout 90s ./release/xl check "release/extlibs/snappy/test/$case.xl" > "$work/$case.txt" 2>&1 || status=$?
    if ((status != 1)); then
        cat "$work/$case.txt" >&2
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

compile -c test/support/extlib_codec_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/snappy/xrt_snappy.c -o "$work/core.o"
compile -c release/extlibs/snappy/snappy_native.c -o "$work/bridge.o"
timeout 180s "$cc" -std=c11 -D_GNU_SOURCE -O1 -g "${sanitize[@]}" \
    -c release/extlibs/snappy/third_party/xrt_snappy.c -o "$work/vendor.o"
objects=("$work/runtime.o" "$work/core.o" "$work/bridge.o" "$work/vendor.o")
compile -DNDEBUG -DSNAPPY_MEMORY_GUARD \
    release/extlibs/snappy/test/test_snappy.c \
    "${objects[@]}" -lm -ldl -o "$work/test_snappy"
compile release/extlibs/snappy/test/test_allocations.c \
    "${objects[@]}" -lm -ldl -o "$work/test_allocations"
compile release/extlibs/snappy/examples/basic.c \
    "${objects[@]}" -lm -ldl -o "$work/basic"
timeout 90s "$work/test_snappy"
timeout 30s "$work/test_allocations"
compile release/extlibs/snappy/test/io_errors_c.c "${objects[@]}" -lm -ldl -o "$work/io_errors"
timeout 90s "$work/io_errors"
timeout 90s ./release/xl run release/extlibs/snappy/test/io_errors_c.c
compile release/extlibs/snappy/test/file_rollback_c.c "${objects[@]}" -lm -ldl -o "$work/rollback"
timeout 90s "$work/rollback"
timeout 90s ./release/xl run release/extlibs/snappy/test/file_rollback_c.c \
    release/extlibs/snappy/xrt_snappy.c release/extlibs/snappy/snappy_native.c \
    release/extlibs/snappy/third_party/xrt_snappy.c --inc-path lib/xrt --inc-path lib/xruntime
compile release/extlibs/snappy/test/empty_chunks_c.c "${objects[@]}" -lm -ldl -o "$work/empty_chunks"
timeout 90s "$work/empty_chunks"
timeout 90s ./release/xl run release/extlibs/snappy/test/empty_chunks_c.c
timeout 90s ./release/xl run release/extlibs/snappy/test/empty_chunks.xl
timeout 90s ./release/xl run release/extlibs/snappy/test/abi_surface.xl
timeout 90s ./release/xl cgen release/extlibs/snappy/test/abi_surface.xl --emit "$work/generated.c"
python3 -I release/extlibs/snappy/test/make_abi_probe.py "$work/generated.c" "$work"
compile -fsyntax-only "$work/abi.c"
status=0
compile -fsyntax-only "$work/abi_wrong.c" > "$work/abi_wrong.txt" 2>&1 || status=$?
test "$status" -eq 1
grep -q 'conflicting types' "$work/abi_wrong.txt"
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
compile release/extlibs/snappy/examples/framed_records_c.c "${objects[@]}" -lm -ldl -o "$work/records"
timeout 30s "$work/records" > "$work/records_c.txt"
diff -u <(sed 's/\r$//' release/extlibs/snappy/examples/framed_records.expected.txt) "$work/records_c.txt"
timeout 90s ./release/xl run release/extlibs/snappy/examples/framed_records_c.c > "$work/records_c_jit.txt"
timeout 90s ./release/xl run release/extlibs/snappy/examples/framed_records.xl > "$work/records_xl_jit.txt"
diff -u "$work/records_c.txt" "$work/records_c_jit.txt"
diff -u "$work/records_c.txt" "$work/records_xl_jit.txt"
compile release/extlibs/snappy/examples/file_transaction_c.c "${objects[@]}" -lm -ldl -o "$work/transaction_c"
timeout 90s "$work/transaction_c" > "$work/transaction.gcc.txt"
diff -u release/extlibs/snappy/examples/file_transaction.expected.txt "$work/transaction.gcc.txt"
for source in file_transaction.xl file_transaction_c.c; do
    timeout 90s ./release/xl run "release/extlibs/snappy/examples/$source" > "$work/$source.jit.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.jit.txt"
    timeout 180s ./release/xl build "release/extlibs/snappy/examples/$source" -o "$work/$source.aot"
    timeout 90s "$work/$source.aot" > "$work/$source.aot.txt"
    diff -u "$work/transaction.gcc.txt" "$work/$source.aot.txt"
done
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
for source in test/empty_chunks.xl test/empty_chunks_c.c test/abi_surface.xl test/io_errors_c.c examples/basic.c examples/framed_records_c.c examples/framed_records.xl; do
    target="$work/$(basename "$source").aot"
    timeout 180s ./release/xl build "release/extlibs/snappy/$source" -o "$target"
    timeout 90s "$target" > "$target.txt"
    case "$source" in
        examples/framed_records*) diff -u "$work/records_c.txt" "$target.txt" ;;
        examples/basic.c) diff -u "$work/basic_c.txt" "$target.txt" ;;
    esac
done
timeout 180s ./release/xl build release/extlibs/snappy/test/file_rollback_c.c \
    release/extlibs/snappy/xrt_snappy.c release/extlibs/snappy/snappy_native.c \
    release/extlibs/snappy/third_party/xrt_snappy.c --inc-path lib/xrt --inc-path lib/xruntime -o "$work/rollback_aot"
timeout 90s "$work/rollback_aot"
printf '[snappy] runtime/ABI and seventeen ordinary AOT paths: PASS\n'
failed=0
status=0
timeout 90s "$work/rollback" --closed > "$work/closed.txt" 2>&1 || status=$?
printf 'closed_rollback\t%s\n' "$status" > "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/closed.txt" >&2; failed=1; fi
status=0
timeout 240s bash "$root/tools/check_extlib_native_globals.sh" snappy > "$work/lifecycle.txt" 2>&1 || status=$?
printf 'top_level_lifecycle\t%s\n' "$status" >> "$work/boundary-status.tsv"
if ((status != 0)); then cat "$work/lifecycle.txt" >&2; failed=1; fi
test "$failed" -eq 0
printf 'snappy Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
