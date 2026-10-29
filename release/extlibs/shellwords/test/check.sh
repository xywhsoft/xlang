#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-shellwords-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/shellwords -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/shellwords/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/shellwords/test/test_shellwords.c \
    release/extlibs/shellwords/shellwords_native.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_shellwords"
timeout 90s "$work/test_shellwords"
"$cc" "${flags[@]}" release/extlibs/shellwords/examples/basic.c \
    "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u release/extlibs/shellwords/examples/basic.expected.txt "$work/example.txt"
if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for shellwords XLang gate\n' >&2
    exit 1
fi
timeout 30s release/xl run release/extlibs/shellwords/examples/basic.c \
    > "$work/example_direct_c.txt"
diff -u "$work/example.txt" "$work/example_direct_c.txt"
(
    cd release
    timeout 90s ./xl run ../test/cases/extlib_shellwords.xl
    timeout 90s ./xl run extlibs/shellwords/test/boundaries.xl
    timeout 90s ./xl run extlibs/shellwords/test/allocations.xl
    timeout 90s ./xl run examples/extlibs/shellwords.xl > "$work/example_xl.txt"
)
diff -u release/extlibs/shellwords/examples/basic.expected.txt "$work/example_xl.txt"
diff -u "$work/example.txt" "$work/example_xl.txt"
printf 'shellwords Linux C ASan/UBSan and XLang parity gate: PASS\n'
