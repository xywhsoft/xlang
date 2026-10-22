#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-crontab-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/crontab/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/crontab/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for name in test_crontab test_dst test_half_hour_dst test_long_horizon; do
    "$cc" "${flags[@]}" release/extlibs/crontab/crontab_native.c \
        "release/extlibs/crontab/test/$name.c" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/$name"
done
timeout 90s "$work/test_crontab"
TZ=America/New_York timeout 90s "$work/test_dst"
TZ=Australia/Lord_Howe timeout 90s "$work/test_half_hour_dst"
timeout 90s "$work/test_long_horizon"

"$cc" "${flags[@]}" release/extlibs/crontab/crontab_native.c \
    release/extlibs/crontab/examples/crontab.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/crontab_example"
timeout 90s "$work/crontab_example" > "$work/crontab_c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'crontab Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_crontab.xl
timeout 90s "$xl" run test/cases/extlib_crontab_boundaries.xl
timeout 90s "$xl" run release/extlibs/crontab/test/allocations.xl
TZ=America/New_York timeout 90s "$xl" run release/extlibs/crontab/test/historical_timezone.xl
TZ=Australia/Lord_Howe timeout 90s "$xl" run release/extlibs/crontab/test/half_hour_timezone.xl
timeout 90s "$xl" run release/examples/extlibs/crontab.xl > "$work/crontab_xl.txt"
timeout 90s "$xl" run release/extlibs/crontab/examples/crontab.c > "$work/crontab_direct_c.txt"
diff -u "$work/crontab_c.txt" "$work/crontab_direct_c.txt"
diff -u "$work/crontab_c.txt" "$work/crontab_xl.txt"
printf 'crontab Linux C/XL ASan/UBSan and DST gate: PASS\n'
