#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-semver-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/semver -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/semver/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/semver/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/semver/semver_native.c \
    release/extlibs/semver/test/test_semver.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_semver"
timeout 90s "$work/test_semver"
"$cc" "${flags[@]}" release/extlibs/semver/examples/semver_c.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u release/extlibs/semver/examples/basic.expected.txt "$work/example.txt"

if [[ ! -x ./release/xl ]]; then
    printf 'semver: Linux release/xl is missing\n' >&2
    exit 1
fi
timeout 30s ./release/xl run release/extlibs/semver/examples/semver_c.c \
    > "$work/direct_c.txt"
diff -u "$work/example.txt" "$work/direct_c.txt"
timeout 45s ./release/xl run test/cases/extlib_semver.xl
timeout 45s ./release/xl run release/extlibs/semver/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/semver/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/semver/examples/basic.xl > "$work/xl.txt"
diff -u release/extlibs/semver/examples/basic.expected.txt "$work/xl.txt"
diff -u "$work/example.txt" "$work/xl.txt"
printf 'semver Linux C ASan/UBSan and XLang parity gate: PASS\n'
