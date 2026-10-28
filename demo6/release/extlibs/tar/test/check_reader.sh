#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work_root=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
work=$(mktemp -d "$work_root/xlang-tar-reader-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$work" in "$work_root"/xlang-tar-reader-*) rm -rf -- "$work";; *) exit 1;; esac
    else
        printf 'TAR reader failure artifacts retained: %s\n' "$work" >&2
    fi
}
trap cleanup EXIT
count=0
run() {
    label=$1
    shift
    count=$((count + 1))
    printf '\n[%s] ' "$label" >> "$work/commands.log"
    printf '%s ' "$@" >> "$work/commands.log"
    printf '\n' >> "$work/commands.log"
    if "$@" > "$work/run-$count.log" 2>&1; then
        cat "$work/run-$count.log" >> "$work/commands.log"
        cat "$work/run-$count.log"
        printf '[tar reader] %s: PASS\n' "$label"
    else
        code=$?
        cat "$work/run-$count.log" >> "$work/commands.log"
        printf 'exit=%s\n' "$code" >> "$work/commands.log"
        cat "$work/commands.log" >&2
        return 1
    fi
}
find "$root/release/extlibs/tar" "$root/release/extlibs/gzip" \
    "$root/release/extlibs/bz2" "$root/release/extlibs/_bzip2" \
    "$root/release/extlibs/lzma" "$root/release/include" \
    -type f -exec sha256sum {} + > "$work/inputs.sha256"
sha256sum "$root/lib/xrt/xrt.h" "$root/lib/xruntime/xruntime.h" \
    "$root/release/xl" "$root/release/libxl.so" "$root/release/libxlc.so" \
    >> "$work/inputs.sha256"
cd "$root"
cc=${CC:-cc}
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
sources='release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/test/runtime.c'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run 'C compiler version' "$cc" --version
run 'C reader sanitizer build' "$cc" $flags -DXRT_MODULE_MEMORY_DEBUG \
    -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/test/test_reader_allocations.c -pthread -lm -o "$work/reader_c"
run 'C faults, snapshots and extension tails' "$work/reader_c" "$work"
run 'XLang allocation and extension boundaries JIT' timeout 90s ./release/xl run \
    release/extlibs/tar/test/reader_allocations.xl -- "$work"
run 'XLang allocation AOT build' timeout 90s ./release/xl build \
    release/extlibs/tar/test/reader_allocations.xl -o "$work/reader_xl"
run 'XLang allocation and extension boundaries AOT' timeout 90s "$work/reader_xl" "$work"
sha256sum -c "$work/inputs.sha256" > "$work/inputs.recheck"
printf '[tar reader] Linux sanitizer/C/JIT/AOT checks: PASS\n'
