#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-daemon-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/daemon/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)

"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -fsyntax-only \
    -Ilib/xrt -Irelease/extlibs/daemon/include \
    -DXRT_MODULE_ATOMIC -DXRT_MODULE_FILE_LOCK -DXRT_MODULE_PATH \
    -DXRT_MODULE_STRING_FORMAT -DXRT_MODULE_UNICODE \
    release/extlibs/daemon/daemon_native.c

"$cc" "${flags[@]}" release/extlibs/daemon/test/xrt_runtime.c \
    release/extlibs/daemon/daemon_native.c \
    release/extlibs/daemon/test/test_daemon.c \
    -no-pie -lm -ldl -o "$work/test_daemon"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 120s "$work/test_daemon"

"$cc" "${flags[@]}" release/extlibs/daemon/test/xrt_runtime.c \
    release/extlibs/daemon/daemon_native.c \
    release/examples/extlibs/daemon_c.c \
    -no-pie -lm -ldl -o "$work/daemon_c"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 120s "$work/daemon_c" > "$work/daemon_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for daemon XLang gate\n' >&2
    exit 1
fi
timeout 120s release/xl run test/cases/extlib_daemon.xl
timeout 120s release/xl run release/extlibs/daemon/test/ffi_boundaries.xl
for negative in private_native_negative private_status_negative private_getter_negative unexported_constructor_negative; do
    if timeout 30s release/xl check "release/extlibs/daemon/test/$negative.xl" \
        > "$work/$negative.txt" 2>&1; then
        printf 'daemon: %s unexpectedly compiled\n' "$negative" >&2
        exit 1
    fi
    grep -Eq '\[(sem|import)\]' "$work/$negative.txt"
done
timeout 120s release/xl run release/examples/extlibs/daemon.xl \
    > "$work/daemon_xl.txt"
timeout 120s release/xl run release/examples/extlibs/daemon_c.c \
    > "$work/daemon_direct.txt"
sed -E 's/ pid=[0-9]+$/ pid=<pid>/' "$work/daemon_c.txt" > "$work/daemon_c_normalized.txt"
sed -E 's/ pid=[0-9]+$/ pid=<pid>/' "$work/daemon_xl.txt" > "$work/daemon_xl_normalized.txt"
sed -E 's/ pid=[0-9]+$/ pid=<pid>/' "$work/daemon_direct.txt" > "$work/daemon_direct_normalized.txt"
diff -u "$work/daemon_c_normalized.txt" "$work/daemon_xl_normalized.txt"
diff -u "$work/daemon_c_normalized.txt" "$work/daemon_direct_normalized.txt"
printf 'daemon Linux C ASan/UBSan and XLang parity gate: PASS\n'
