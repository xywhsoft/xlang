#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work_root=$(CDPATH= cd -- "${TMPDIR:-/tmp}" && pwd)
work=$(mktemp -d "$work_root/xlang-tar-writer-XXXXXX")
cleanup() {
    code=$?
    if [ "$code" -eq 0 ]; then
        case "$work" in "$work_root"/xlang-tar-writer-*) rm -rf -- "$work";; *) exit 1;; esac
    else
        printf 'TAR writer failure artifacts retained: %s\n' "$work" >&2
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
        printf '[tar writer] %s: PASS\n' "$label"
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
flags='-std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -DXRT_MODULE_MEMORY_DEBUG'
sources='release/extlibs/tar/xrt_tar_common.c release/extlibs/tar/xrt_tar_reader.c release/extlibs/tar/xrt_tar_writer.c release/extlibs/tar/test/runtime.c'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run 'C compiler version' "$cc" --version
run 'C writer sanitizer build' "$cc" $flags -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/test/test_writer_allocations.c -pthread -lm -o "$work/writer_c"
run 'C faults, snapshots, atomic files and no-allocation destruction' "$work/writer_c" "$work"
run 'existing C TAR sanitizer build' "$cc" $flags -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/xrt_tar_extract.c release/extlibs/tar/test/test_tar_c.c \
    -pthread -lm -o "$work/test_tar"
run 'existing C TAR suite' "$work/test_tar"
run 'XLang allocation and transaction boundaries JIT' timeout 90s ./release/xl run \
    release/extlibs/tar/test/writer_allocations.xl -- "$work"
run 'JIT temporary cleanup' "$work/writer_c" "$work" --check-clean
run 'XLang allocation AOT build' timeout 90s ./release/xl build \
    release/extlibs/tar/test/writer_allocations.xl -o "$work/writer_xl"
run 'XLang allocation and transaction boundaries AOT' timeout 90s "$work/writer_xl" "$work"
run 'AOT temporary cleanup' "$work/writer_c" "$work" --check-clean
run 'closed transaction sanitizer build' "$cc" $flags -Ilib/xrt -Irelease/extlibs/tar $sources \
    release/extlibs/tar/test/closed_transaction_oom.c -pthread -lm -o "$work/closed"
sha256sum -c "$work/inputs.sha256" > "$work/inputs.recheck"
run 'closed transaction rollback OOM' "$work/closed" "$work"
printf '[tar writer] Linux sanitizer/C/JIT/AOT checks: PASS\n'
