#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-serial-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

flags='-std=c11 -D_DEFAULT_SOURCE -DNDEBUG -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -pthread -Ilib/xrt'
links='-fsanitize=address,undefined -pthread -ldl -lm -lutil'

# 独立的 XRT 实现对象只用于 C 门禁；包本体始终复用宿主 XRT。
gcc $flags -c release/extlibs/serial/test/serial_runtime.c -o "$work/runtime.o"
gcc $flags -c release/extlibs/serial/serial_native.c -o "$work/native.o"
for test in test_serial test_serial_alloc test_serial_pty; do
    gcc $flags "release/extlibs/serial/test/$test.c" "$work/native.o" "$work/runtime.o" $links -o "$work/$test"
    "$work/$test"
done

gcc $flags release/extlibs/serial/examples/device_c.c "$work/native.o" "$work/runtime.o" $links -o "$work/device_c"
gcc $flags -DNDEBUG release/examples/extlibs/serial_c.c "$work/runtime.o" $links -o "$work/loopback_c"
"$work/loopback_c" > "$work/loopback_c.txt"
if [ -x release/xl ] && command -v python3 >/dev/null 2>&1; then
    ./release/xl build release/extlibs/serial/examples/device_c.c -o "$work/device_direct"
    (cd release && ./xl run examples/extlibs/serial.xl) > "$work/loopback_xl.txt"
    diff -u "$work/loopback_c.txt" "$work/loopback_xl.txt"
    (cd release && ./xl run ../test/cases/extlib_serial.xl)
    (cd release && ./xl run extlibs/serial/test/nul_port.xl)
    for method in text bytes; do
        case="private_${method}_negative"
        if (cd release && ./xl check "extlibs/serial/test/$case.xl") \
            > "$work/$case.txt" 2>&1; then
            printf 'serial: %s must be rejected\n' "$case" >&2
            exit 1
        fi
        expected=writeNative
        if [ "$method" = bytes ]; then expected=writeBytesNative; fi
        if ! grep -Fq "$expected" "$work/$case.txt" ||
           ! grep -Fq 'access' "$work/$case.txt"; then
            cat "$work/$case.txt" >&2
            printf 'serial: expected private-access diagnostic\n' >&2
            exit 1
        fi
    done
    error_output=$(cd release && ./xl run extlibs/serial/test/posix_error_probe.xl)
    if [ "$error_output" != "$(printf 'FileError\nxrt.file\nopen')" ]; then
        printf 'serial POSIX open error contract changed: %s\n' "$error_output" >&2
        exit 1
    fi
    python3 release/extlibs/serial/test/check_pty.py "$root/release" "$work/device_c" "$work/device_direct"
fi
