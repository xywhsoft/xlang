#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-procps-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/procps
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)

"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -x c -fsyntax-only release/extlibs/procps/include/procps_native.h
"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -DXRT_MODULE_VALUE_CONTAINER -x c -fsyntax-only \
    release/extlibs/procps/include/procps_native.h

"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -fsyntax-only \
    -Ilib/xrt -Irelease/extlibs/procps -DXRT_MODULE_VALUE_CONTAINER \
    -DXRT_MODULE_FILE_WHOLE -DXRT_MODULE_UNICODE \
    -DXRT_MODULE_NUMBER_INTEGER \
    release/extlibs/procps/procps_native.c

"$cc" "${flags[@]}" -c release/extlibs/procps/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/procps/procps_native.c \
    release/extlibs/procps/test/test_procps.c \
    -no-pie -lm -ldl -o "$work/test_procps"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
timeout 120s "$work/test_procps"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/procps/procps_native.c \
    release/extlibs/procps/test/long_exe_name.c \
    -Wl,--wrap=readlink -Wl,--wrap=__readlink_chk \
    -no-pie -lm -ldl -o "$work/long_exe_name"
timeout 120s "$work/long_exe_name"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/procps/procps_native.c \
    release/extlibs/procps/test/statm_malformed.c \
    -Wl,--wrap=xrtFileReadAllLimit -no-pie -lm -ldl \
    -o "$work/statm_malformed"
timeout 120s "$work/statm_malformed"
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/procps/procps_native.c \
    release/extlibs/procps/test/name_fallback.c \
    -Wl,--wrap=readlink -Wl,--wrap=__readlink_chk \
    -Wl,--wrap=xrtFileReadAllLimit -no-pie -lm -ldl \
    -o "$work/name_fallback"
timeout 120s "$work/name_fallback"
bad="$work/procps-"$'\xff'
mv -- "$work/test_procps" "$bad"
"$bad" --invalid-name
"$cc" "${flags[@]}" "$work/xrt_runtime.o" \
    release/extlibs/procps/procps_native.c \
    release/extlibs/procps/examples/procps_c.c \
    -no-pie -lm -ldl -o "$work/procps_example"
timeout 30s "$work/procps_example" > "$work/c_example.txt"
diff -u release/extlibs/procps/test/procps.expected.txt "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/procps/examples/procps_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 180s ./release/xl run release/extlibs/procps/test/boundaries.xl
timeout 180s ./release/xl run test/cases/extlib_procps.xl
timeout 180s ./release/xl run release/examples/extlibs/procps.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"

timeout 180s ./release/xl build test/cases/extlib_procps.xl -o "$work/procps_core"
timeout 30s "$work/procps_core"
timeout 180s ./release/xl build release/extlibs/procps/test/boundaries.xl -o "$work/procps_boundaries"
timeout 30s "$work/procps_boundaries"
timeout 180s ./release/xl build release/examples/extlibs/procps.xl -o "$work/procps_xl_aot"
timeout 30s "$work/procps_xl_aot" > "$work/xl_aot.txt"
diff -u release/extlibs/procps/test/procps.expected.txt "$work/xl_aot.txt"
timeout 180s ./release/xl build release/extlibs/procps/examples/procps_c.c -o "$work/procps_c_aot"
timeout 30s "$work/procps_c_aot" > "$work/c_aot.txt"
diff -u release/extlibs/procps/test/procps.expected.txt "$work/c_aot.txt"
printf 'procps Linux C ASan/UBSan, XLang parity and bilingual AOT gate: PASS\n'
