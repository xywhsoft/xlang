#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-tar-text-XXXXXXXX")
phase=setup
finish() {
    status=$?
    trap - EXIT
    printf '%s\n' "$status" > "$work/exit-code.txt"
    printf '%s\n' "$phase" > "$work/last-phase.txt"
    printf '[tar text] exit=%s artifacts=%s\n' "$status" "$work" >&2
    exit "$status"
}
trap finish EXIT
run() {
    phase=$1
    shift
    local status=0
    printf '%q ' "$@" > "$work/$phase.command"
    printf '\n' >> "$work/$phase.command"
    "$@" > "$work/$phase.stdout" 2> "$work/$phase.stderr" || status=$?
    cat "$work/$phase.stdout"
    cat "$work/$phase.stderr" >&2
    printf '%s\t%s\n' "$phase" "$status" >> "$work/phases.tsv"
    return "$status"
}
mapfile -d '' inputs < <({
    find release/extlibs/{tar,gzip,_zlib,bz2,_bzip2,lzma} -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
} | sort -zu)
run fingerprint sha256sum "${inputs[@]}"
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -DXRT_MODULE_MEMORY_DEBUG
    -Ilib/xrt -Irelease/extlibs/tar -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
pkg=release/extlibs/tar
run c-compile "$cc" "${flags[@]}" "$pkg/test/test_text_metadata.c" "$pkg/test/runtime.c" -lm -ldl -o "$work/matrix"
run c-matrix timeout 90s "$work/matrix"
run core-compile "$cc" "${flags[@]}" "$pkg/test/test_tar_c.c" "$pkg/xrt_tar_common.c" \
    "$pkg/xrt_tar_reader.c" "$pkg/xrt_tar_writer.c" "$pkg/xrt_tar_extract.c" "$pkg/test/runtime.c" -lm -ldl -o "$work/core"
mkdir "$work/run"
run core-run bash -c 'cd "$1" && timeout 180s "$2"' -- "$work/run" "$work/core"
run c-jit timeout 90s release/xl run "$pkg/test/test_text_metadata.c"
run xl-jit timeout 90s release/xl run "$pkg/test/text_metadata.xl"
run example-compile "$cc" "${flags[@]}" "$pkg/examples/metadata_hex_c.c" "$pkg/test/runtime.c" -lm -ldl -o "$work/metadata"
for source in test/text_metadata.xl test/test_text_metadata.c examples/metadata_hex.xl examples/metadata_hex_c.c; do
    name=${source##*/}
    run "build-$name" timeout 120s release/xl build "$pkg/$source" -o "$work/$name.exe"
    if [[ $source == test/* ]]; then run "run-$name" timeout 90s "$work/$name.exe"; fi
done
run oracle timeout 180s python3 -B "$pkg/test/text_metadata.py" release "$work/metadata" "$work/oracle" \
    --xl-aot "$work/metadata_hex.xl.exe" --c-aot "$work/metadata_hex_c.c.exe"
run fingerprint-after sha256sum "${inputs[@]}"
run unchanged diff -u "$work/fingerprint.stdout" "$work/fingerprint-after.stdout"
printf 'TAR text C/XL and independent oracle: PASS\n'
