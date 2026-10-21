#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-volc-core-XXXXXX")
trap 'rm -f -- "$work/xrt_runtime.o" "$work/test_volc_core" "$work/signing" "$work/test.out" "$work/signing.out"; rmdir -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Ilib/xruntime -Irelease/include
       -Irelease/extlibs/xhttp.client/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -ffunction-sections -fdata-sections)
link=(-Wl,--gc-sections -pthread -lm -ldl)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/volc.core/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -DNDEBUG \
    release/extlibs/volc.core/test/test_volc_core.c "$work/xrt_runtime.o" \
    "${link[@]}" -o "$work/test_volc_core"
timeout 90s "$work/test_volc_core" > "$work/test.out"
printf 'volc.core C smoke: PASS\n' | diff -u - "$work/test.out"

"$cc" "${flags[@]}" -DXRT_MODULE_ALL \
    release/extlibs/volc.core/examples/signing.c \
    release/extlibs/volc.core/x_volc_core_native.c "$work/xrt_runtime.o" \
    "${link[@]}" -o "$work/signing"
timeout 30s "$work/signing" > "$work/signing.out"
printf 'volc.core extlib ok\n' | diff -u - "$work/signing.out"
printf 'volc.core Linux C ASan/UBSan offline gate: PASS\n'
