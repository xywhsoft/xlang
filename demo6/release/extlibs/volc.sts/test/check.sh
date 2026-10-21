#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-volc-sts-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/include
       -Irelease/extlibs/volc.core/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -ffunction-sections -fdata-sections -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/volc.sts/test/xrt_runtime.c -o "$work/xrt.o"
"$cc" "${flags[@]}" -DXRT_MODULE_ALL -DXRUNTIME_MODULE_ALL \
    -c release/extlibs/volc.core/x_volc_core_native.c -o "$work/core.o"
for name in test/test_sts examples/assume_role; do
    output="$work/$(basename "$name")"
    "$cc" "${flags[@]}" "release/extlibs/volc.sts/$name.c" \
        "$work/core.o" "$work/xrt.o" -Wl,--gc-sections -lm -ldl -o "$output"
    timeout 60s "$output"
done
printf 'volc.sts Linux ASan/UBSan gate: PASS\n'
