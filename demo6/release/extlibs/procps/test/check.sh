#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-procps-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[procps] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/procps
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
mapfile -d '' inputs < <(find release/extlibs/procps release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_procps.xl release/examples/extlibs/procps.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
golden="$work/procps.expected.txt"
sed 's/\r$//' release/extlibs/procps/test/procps.expected.txt > "$golden"

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
diff -u "$golden" "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/procps/examples/procps_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
timeout 180s ./release/xl run release/extlibs/procps/test/boundaries.xl
timeout 180s ./release/xl run test/cases/extlib_procps.xl
timeout 180s ./release/xl run release/extlibs/procps/test/owned_results.xl
timeout 180s ./release/xl cgen release/extlibs/procps/test/owned_probe.xl --emit "$work/owned_probe.c"
count=$(grep -Ec '[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xprocps_name[[:space:]]*\(' \
    "$work/owned_probe.c")
if [[ $count != 1 ]] || grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'procps: nullable success must be transferred without copying\n' >&2
    exit 1
fi
timeout 180s ./release/xl run release/examples/extlibs/procps.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"

timeout 180s ./release/xl build test/cases/extlib_procps.xl -o "$work/procps_core"
timeout 30s "$work/procps_core"
timeout 180s ./release/xl build release/extlibs/procps/test/boundaries.xl -o "$work/procps_boundaries"
timeout 30s "$work/procps_boundaries"
timeout 180s ./release/xl build release/extlibs/procps/test/owned_results.xl -o "$work/procps_owned"
timeout 30s "$work/procps_owned"
timeout 180s ./release/xl build release/examples/extlibs/procps.xl -o "$work/procps_xl_aot"
timeout 30s "$work/procps_xl_aot" > "$work/xl_aot.txt"
diff -u "$golden" "$work/xl_aot.txt"
timeout 180s ./release/xl build release/extlibs/procps/examples/procps_c.c -o "$work/procps_c_aot"
timeout 30s "$work/procps_c_aot" > "$work/c_aot.txt"
diff -u "$golden" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'procps Linux C ASan/UBSan, XLang parity and bilingual AOT gate: PASS\n'
