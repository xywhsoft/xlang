#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-singleinstance-XXXXXX")
finish() {
    result=$?
    if [ "$result" = 0 ];then rm -rf -- "$work";else printf 'context failure artifacts: %s\n' "$work" >&2;fi
    exit "$result"
}
trap finish EXIT
cd "$root"
mkdir "$work/home";chmod 0700 "$work/home"
export HOME="$work/home" XLANG_SI_TEST_HOME="$work/home"

gcc -std=c11 -O2 -Wall -Wextra -Werror -Irelease/include -Ilib/xrt \
    release/extlibs/singleinstance/test/test_contexts.c -Lrelease -lxl \
    "-Wl,-rpath,$root/release" -pthread -o "$work/context_test"
XLANG_PACKAGE_PATH="$root/release/extlibs" timeout 120s "$work/context_test" "$root/release" \
    "$root/release/extlibs/singleinstance/test/context_hold.xl" \
    "$root/release/extlibs/singleinstance/test/context_probe.xl" \
    "$root/release/extlibs/singleinstance/test/context_after.xl" "$work/ready" "$work/done"
