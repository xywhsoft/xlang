#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-netrc-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/netrc -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -DXRT_MODULE_VALUE_CONTAINER -fsyntax-only \
    release/extlibs/netrc/netrc_native.c
"$cc" "${flags[@]}" -c release/extlibs/netrc/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for name in test_netrc test_netrc_boundaries; do
    "$cc" "${flags[@]}" "release/extlibs/netrc/test/$name.c" \
        "$work/xrt_runtime.o" -lm -ldl -o "$work/$name"
    ( cd "$work" && timeout 90s "$work/$name" )
done
"$cc" "${flags[@]}" release/extlibs/netrc/examples/basic.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example.txt"
diff -u release/extlibs/netrc/examples/basic.expected.txt "$work/example.txt"
timeout 30s "$work/example" "$work/c.netrc" > "$work/example_file_c.txt"
diff -u release/extlibs/netrc/examples/basic.expected.txt "$work/example_file_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'netrc: Linux xl runner is missing: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_netrc.xl
timeout 90s "$xl" run release/extlibs/netrc/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/netrc/test/named_type.xl
timeout 90s "$xl" run release/examples/extlibs/netrc.xl > "$work/example_xl.txt"
diff -u release/extlibs/netrc/examples/basic.expected.txt "$work/example_xl.txt"
diff -u "$work/example.txt" "$work/example_xl.txt"
timeout 90s "$xl" run release/examples/extlibs/netrc_c.c > "$work/example_central.txt"
diff -u release/extlibs/netrc/examples/basic.expected.txt "$work/example_central.txt"
timeout 90s "$xl" run release/examples/extlibs/netrc.xl -- "$work/xl.netrc" > "$work/example_file_xl.txt"
diff -u "$work/example_file_c.txt" "$work/example_file_xl.txt"
cmp "$work/c.netrc" "$work/xl.netrc"
[[ $(stat -c %a "$work/c.netrc") == 600 && $(stat -c %a "$work/xl.netrc") == 600 ]]
if command -v python3 >/dev/null 2>&1; then
    timeout 30s python3 release/extlibs/netrc/test/interop.py \
        "$work/c.netrc" "$work/xl.netrc"
else
    printf 'netrc: Python 3 unavailable; skipping independent file interop check\n' >&2
fi
printf 'netrc Linux C ASan/UBSan and XLang parity gate: PASS\n'
