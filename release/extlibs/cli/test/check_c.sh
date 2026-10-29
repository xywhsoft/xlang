#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cli-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/cli/test/xrt_runtime.c -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/cli/cli_native.c \
    release/extlibs/cli/test/test_cli.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_cli"
timeout 90s "$work/test_cli"
"$cc" "${flags[@]}" release/extlibs/cli/cli_native.c \
    release/extlibs/cli/examples/basic_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u release/extlibs/cli/examples/basic.expected.txt "$work/example.txt"
timeout 30s ./release/xl run release/extlibs/cli/examples/basic_c.c > "$work/example_direct.txt"
diff -u "$work/example.txt" "$work/example_direct.txt"
printf 'cli Linux C ASan/UBSan gate: PASS\n'
