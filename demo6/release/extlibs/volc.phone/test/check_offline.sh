#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-volc-phone-XXXXXX")
trap 'rm -f -- "$work/xrt.o" "$work/preview" "$work/out.txt"; rmdir -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/include
       -Irelease/extlibs/xhttp.client/include -Irelease/extlibs/volc.core/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -ffunction-sections -fdata-sections)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
"$cc" "${flags[@]}" -c release/extlibs/volc.core/test/xrt_runtime.c -o "$work/xrt.o"
"$cc" "${flags[@]}" -DXRT_MODULE_ALL \
    release/extlibs/volc.phone/examples/preview.c \
    release/extlibs/volc.core/x_volc_core_native.c "$work/xrt.o" \
    -Wl,--gc-sections -pthread -lm -ldl -o "$work/preview"
timeout 30s "$work/preview" > "$work/out.txt"
printf 'volc.phone offline example: ok\n' | diff -u - "$work/out.txt"
python3 release/extlibs/volc.phone/test/check_response.py
printf 'volc.phone Linux offline gate: PASS (full package import not tested)\n'
