#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cbor-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/cbor release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_cbor.xl release/examples/extlibs/cbor.xl
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
    if [[ -n "$python_path" ]]; then printf '%s\0' "$python_path"; fi
}
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\nphases=%s\n' "$result" "$sequence" > "$work.report.txt"
    if [[ -f "$work/commands.log" ]]; then cat "$work/commands.log" >> "$work.report.txt"; fi
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[cbor] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[cbor] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s line=%s command=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run() {
    local seconds=$1 result
    shift
    sequence=$((sequence+1))
    printf '\n$ timeout %ss' "$seconds" >> "$work/commands.log"
    printf ' %q' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if timeout "${seconds}s" "$@" > "$work/$sequence.out" 2> "$work/$sequence.err"; then result=0; else result=$?; fi
    printf 'exit=%s\n' "$result" >> "$work/commands.log"
    cat "$work/$sequence.out" "$work/$sequence.err" >> "$work/commands.log"
    cat "$work/$sequence.out"
    cat "$work/$sequence.err" >&2
    return "$result"
}
run 10 "$cc" --version > "$work/compiler.txt"
run 10 "$python" --version
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/cbor
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/cbor/cbor_native.h
compile -DXRT_MODULE_BUFFER -DXRT_MODULE_BUFFER_HEX -DXRT_MODULE_CODEC_HEX -DXRT_MODULE_STRING \
    -DXRT_MODULE_TIME_TEXT -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER -fsyntax-only release/extlibs/cbor/cbor_native.c
compile -c release/extlibs/cbor/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/cbor/cbor_native.c -o "$work/native.o"
for name in test_cbor test_boundaries test_allocations wellformed_c; do
    compile -DNDEBUG "release/extlibs/cbor/test/$name.c" "$work/native.o" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'cbor well-formedness: 2591 checks, 0 failures\n' > "$work/checks.expected"
diff -u "$work/checks.expected" "$work/wellformed_c.txt"
run 90 "$work/wellformed_c" --half-records > "$work/half.tsv"
run 30 "$python" release/extlibs/cbor/test/check_half.py "$work/half.tsv"
run 120 ./release/xl run release/extlibs/cbor/test/wellformed_c.c > "$work/wire.direct"
diff -u "$work/checks.expected" "$work/wire.direct"
sources=(release/extlibs/cbor/test/aot_core.xl release/extlibs/cbor/test/wellformed.xl)
for source in "${sources[@]}"; do run 120 ./release/xl run "$source"; done
for name in old wire; do
    objects=("$work/runtime.o")
    if [[ $name == old ]]; then
        c=release/extlibs/cbor/examples/cbor_c.c; expected=release/extlibs/cbor/examples/basic.expected.txt
    else
        c=release/extlibs/cbor/examples/validate_wire_c.c; expected=release/extlibs/cbor/examples/validate_wire.expected.txt
        objects+=("$work/native.o")
    fi
    sed 's/\r$//' "$expected" > "$work/$name.expected"
    compile -DNDEBUG "$c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$c" > "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
done
run 90 ./release/xl run release/extlibs/cbor/examples/validate_wire.xl > "$work/wire.xl.txt"
diff -u "$work/wire.expected" "$work/wire.xl.txt"
run 90 ./release/xl cgen release/extlibs/cbor/examples/validate_wire.xl --emit "$work/probe.c"
grep -E '^extern .*xcbor_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xcbor_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
printf '%s\n' xcbor_decode_abi xcbor_decode_bytes_abi xcbor_encode_abi xcbor_encode_bytes_abi \
    xcbor_error_message xcbor_failed xcbor_from_hex_abi xcbor_to_hex_abi > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 8
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "cbor_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
for source in "${sources[@]}" release/extlibs/cbor/test/wellformed_c.c release/extlibs/cbor/examples/cbor_c.c \
    release/extlibs/cbor/examples/validate_wire.xl release/extlibs/cbor/examples/validate_wire_c.c; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    run 180 ./release/xl build "$source" -o "$target"
    run 120 "$target" > "$target.out"
    case "$source" in
        *validate_wire*) diff -u "$work/wire.expected" "$target.out" ;;
        */examples/cbor_c.c) diff -u "$work/old.expected" "$target.out" ;;
        */wellformed_c.c) diff -u "$work/wellformed_c.txt" "$target.out" ;;
    esac
done
# 完整旧合同仍为必过正例，不把主线容器投影失败转换成预期成功。
for source in release/extlibs/cbor/test/boundaries.xl release/extlibs/cbor/test/allocations.xl \
    release/extlibs/cbor/examples/cbor.xl release/examples/extlibs/cbor.xl test/cases/extlib_cbor.xl; do
    run 120 ./release/xl run "$source" > "$work/full.txt"
    if [[ $source == release/extlibs/cbor/examples/cbor.xl ]]; then diff -u "$work/old.expected" "$work/full.txt"; fi
    target="$work/$(basename "${source%.*}")-full"
    run 180 ./release/xl build "$source" -o "$target"
    run 120 "$target" > "$target.out"
    if [[ $source == release/extlibs/cbor/examples/cbor.xl ]]; then diff -u "$work/old.expected" "$target.out"; fi
done
printf 'cbor complete C/XLang/AOT gate: PASS\n'
