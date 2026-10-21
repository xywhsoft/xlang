#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ical-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/ical/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER \
    release/extlibs/ical/ical_native.c release/extlibs/ical/ical_parse.c \
    release/extlibs/ical/ical_build.c
"$cc" "${flags[@]}" -c release/extlibs/ical/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for unit in ical_native ical_parse ical_build; do
    "$cc" "${flags[@]}" -c "release/extlibs/ical/$unit.c" -o "$work/$unit.o"
done
objects=("$work/xrt_runtime.o" "$work/ical_native.o" "$work/ical_parse.o" "$work/ical_build.o")
"$cc" "${flags[@]}" release/extlibs/ical/test/test_ical.c \
    "${objects[@]}" -lm -ldl -o "$work/test_ical"
"$cc" "${flags[@]}" release/extlibs/ical/examples/ical.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example_ical"
timeout 90s "$work/test_ical" "$work/long.ics"
"$work/example_ical" "$work/c.ics" > "$work/c.txt"
timeout 90s ./release/xl run release/extlibs/ical/examples/ical.c \
    > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"

./release/xl run test/cases/extlib_ical.xl
./release/xl run test/cases/extlib_ical_boundaries.xl
./release/xl run release/extlibs/ical/test/control_characters.xl
./release/xl run release/extlibs/ical/test/allocations.xl
./release/xl run release/extlibs/ical/test/text_identifiers.xl
./release/xl run release/extlibs/ical/test/nested_calendar.xl
./release/xl run release/examples/extlibs/ical.xl -- "$work/xl.ics" > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
python3 release/extlibs/ical/test/check_wire.py "$work/c.ics" "$work/xl.ics" "$work/long.ics"
printf 'ical Linux C ASan/UBSan, XLang and wire gate: PASS\n'
