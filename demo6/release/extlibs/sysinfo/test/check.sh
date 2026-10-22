#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sysinfo-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/sysinfo -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/sysinfo/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/sysinfo/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/test_sysinfo.c \
    -no-pie -lm -ldl -o "$work/test_sysinfo"
timeout 90s "$work/test_sysinfo"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/non_utf8_system_text.c \
    -Wl,--wrap=gethostname -Wl,--wrap=__gethostname_chk \
    -Wl,--wrap=uname -Wl,--wrap=fopen -no-pie -lm -ldl \
    -o "$work/non_utf8_system_text"
timeout 90s "$work/non_utf8_system_text"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/long_cpu_name.c \
    -Wl,--wrap=fopen -no-pie -lm -ldl -o "$work/long_cpu_name"
timeout 90s "$work/long_cpu_name"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/meminfo_unavailable.c \
    -Wl,--wrap=fopen -no-pie -lm -ldl -o "$work/meminfo_unavailable"
timeout 90s "$work/meminfo_unavailable"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/meminfo_malformed.c \
    -Wl,--wrap=fopen -no-pie -lm -ldl -o "$work/meminfo_malformed"
timeout 90s "$work/meminfo_malformed"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/sysinfo_native.c \
    release/extlibs/sysinfo/test/uptime_overflow.c \
    -Wl,--wrap=fopen -no-pie -lm -ldl -o "$work/uptime_overflow"
timeout 90s "$work/uptime_overflow"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/examples/sysinfo_c.c \
    -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/sysinfo/examples/metrics_c.c \
    -no-pie -lm -ldl -o "$work/metrics"
timeout 30s "$work/metrics" > "$work/metrics_c.txt"

if [[ ! -x ./release/xl ]]; then
    printf 'sysinfo: Linux release/xl is missing\n' >&2
    exit 1
fi
timeout 45s ./release/xl run release/extlibs/sysinfo/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/sysinfo/examples/basic.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
timeout 45s ./release/xl run release/extlibs/sysinfo/examples/sysinfo_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
timeout 45s ./release/xl run release/extlibs/sysinfo/examples/metrics.xl > "$work/metrics_xl.txt"
timeout 45s ./release/xl run release/extlibs/sysinfo/examples/metrics_c.c > "$work/metrics_c_direct.txt"
for output in "$work/metrics_c.txt" "$work/metrics_xl.txt" "$work/metrics_c_direct.txt"; do
    test "$(wc -l < "$output")" -eq 3
    grep -Eq '^available: [0-9]+$' "$output"
    grep -Eq '^usage: ([0-9]|[1-9][0-9]|100)$' "$output"
    grep -Eq '^uptime: [0-9]+$' "$output"
done
printf 'sysinfo Linux C ASan/UBSan and XLang parity gate: PASS\n'
