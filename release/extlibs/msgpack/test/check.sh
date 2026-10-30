#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-msgpack-XXXXXX")
cc=${CC:-gcc}; python=${PYTHON:-python3}
cc_path=$(command -v "$cc" || true); python_path=$(command -v "$python" || true)
sequence=0
input_paths() {
    find release/extlibs/msgpack release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_msgpack.xl release/examples/extlibs/msgpack.xl
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
    else printf '[msgpack] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[msgpack] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/msgpack
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { run 180 "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export PYTHONDONTWRITEBYTECODE=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
modules=(-DXRT_MODULE_BUFFER -DXRT_MODULE_BUFFER_HEX -DXRT_MODULE_CODEC_HEX
         -DXRT_MODULE_STRING -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER)
compile "${modules[@]}" -x c -fsyntax-only release/extlibs/msgpack/msgpack_native.h
compile "${modules[@]}" -fsyntax-only release/extlibs/msgpack/msgpack_native.c
compile -c release/extlibs/msgpack/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/msgpack/msgpack_native.c -o "$work/native.o"
for name in test_msgpack test_boundaries test_allocations extensions_c; do
    objects=("$work/runtime.o")
    if [[ $name != extensions_c ]]; then objects+=("$work/native.o"); fi
    compile "release/extlibs/msgpack/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    run 120 "$work/$name" > "$work/$name.txt"
    cat "$work/$name.txt"
done
printf 'msgpack extension framing: 50651 checks, 18 OOM paths, 0 failures\n' > "$work/extensions.expected"
diff -u "$work/extensions.expected" "$work/extensions_c.txt"
run 90 "$work/extensions_c" --records > "$work/gcc.tsv"
run 60 "$python" release/extlibs/msgpack/test/check_timestamps.py "$work/gcc.tsv"
source=release/extlibs/msgpack/test/extensions_c.c
run 120 ./release/xl run "$source" > "$work/extensions.direct"
diff -u "$work/extensions.expected" "$work/extensions.direct"
run 90 ./release/xl run "$source" -- --records > "$work/tcc.tsv"
cmp "$work/gcc.tsv" "$work/tcc.tsv"
run 60 "$python" release/extlibs/msgpack/test/check_timestamps.py "$work/tcc.tsv"
for name in msgpack timestamp_frames; do
    golden=$name
    if [[ $name == msgpack ]]; then golden=basic; fi
    sed 's/\r$//' "release/extlibs/msgpack/examples/$golden.expected.txt" > "$work/$name.expected"
    source="release/extlibs/msgpack/examples/${name}_c.c"
    compile "$source" "$work/runtime.o" -no-pie -lm -ldl -o "$work/$name"
    run 90 "$work/$name" > "$work/$name.c.txt"
    run 90 ./release/xl run "$source" > "$work/$name.direct.txt"
    diff -u "$work/$name.expected" "$work/$name.c.txt"
    diff -u "$work/$name.expected" "$work/$name.direct.txt"
done
if run 30 "$work/msgpack" extra extra; then exit 1; else test "$?" -eq 1; fi
if run 30 "$work/msgpack" "$work/missing/output.bin"; then exit 1; else test "$?" -eq 1; fi
language=release/extlibs/msgpack/examples/timestamp_frames.xl
run 90 ./release/xl run "$language" > "$work/timestamp_frames.xl.txt"
diff -u "$work/timestamp_frames.expected" "$work/timestamp_frames.xl.txt"
paths=release/extlibs/msgpack/test/extensions.xl
run 120 ./release/xl run "$paths"
run 90 ./release/xl cgen "$language" --emit "$work/probe.c"
grep -Eq 'xl6_xrt_take_cstr\(' "$work/probe.c"
grep -E '^extern .*xmsgpack_[a-z_]+[(]' "$work/probe.c" | sort -u > "$work/abi.decls"
grep -oE 'xmsgpack_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
sed -nE 's/^extern "([A-Za-z0-9_]+)".*/\1/p' release/extlibs/msgpack/src/msgpack.xl | sort -u > "$work/abi.expected"
test "$(wc -l < "$work/abi.decls")" -eq 8
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "msgpack_native.c"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
sources=(release/extlibs/msgpack/test/extensions_c.c "$paths" "$language"
         release/extlibs/msgpack/examples/msgpack_c.c release/extlibs/msgpack/examples/timestamp_frames_c.c)
index=0
for source in "${sources[@]}"; do
    index=$((index+1))
    binary="$work/aot-$index"
    run 120 ./release/xl build "$source" -o "$binary"
    run 90 "$binary" > "$work/aot-$index.txt"
    case "$source" in
        */extensions_c.c)
            diff -u "$work/extensions.expected" "$work/aot-$index.txt"
            run 90 "$binary" --records > "$work/aot.tsv"
            cmp "$work/gcc.tsv" "$work/aot.tsv"
            run 60 "$python" release/extlibs/msgpack/test/check_timestamps.py "$work/aot.tsv" ;;
        */msgpack_c.c) diff -u "$work/msgpack.expected" "$work/aot-$index.txt" ;;
        */timestamp_frames_c.c|*/timestamp_frames.xl) diff -u "$work/timestamp_frames.expected" "$work/aot-$index.txt" ;;
    esac
done
# 语言字典必须作为真实正例通过，不能以 C 创建的字典或标量替代。
for source in test/cases/extlib_msgpack.xl release/extlibs/msgpack/test/aot_core.xl \
    release/extlibs/msgpack/test/boundaries.xl release/extlibs/msgpack/test/allocations.xl \
    release/extlibs/msgpack/examples/msgpack.xl release/examples/extlibs/msgpack.xl; do
    run 120 ./release/xl run "$source" > "$work/complete.txt"
    if [[ $source == release/extlibs/msgpack/examples/msgpack.xl ]]; then
        diff -u "$work/msgpack.expected" "$work/complete.txt"
    fi
    run 120 ./release/xl build "$source" -o "$work/complete"
    run 90 "$work/complete"
done
printf 'msgpack Linux sanitizer, independent timestamp oracle, actual ABI and bilingual JIT/AOT: PASS\n'
