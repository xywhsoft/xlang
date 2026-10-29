#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-singleinstance-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

gcc -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include -Ilib/xrt \
    release/extlibs/singleinstance/test/test_contexts.c -Lrelease -lxl \
    "-Wl,-rpath,$root/release" -pthread -o "$work/context_test"
XLANG_PACKAGE_PATH="$root/release/extlibs" "$work/context_test" "$root/release" \
    "$root/release/extlibs/singleinstance/test/context_hold.xl" \
    "$root/release/extlibs/singleinstance/test/context_probe.xl" \
    "$root/release/extlibs/singleinstance/test/context_after.xl"
