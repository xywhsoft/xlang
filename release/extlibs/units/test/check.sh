#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-units-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[units] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/units/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/units release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_units.xl release/examples/extlibs/units.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/units/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/units/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/units/units_native.c \
    -o "$work/units_native.o"
objects=("$work/units_native.o" "$work/xrt_runtime.o")
"$cc" "${flags[@]}" release/extlibs/units/test/test_units.c \
    "${objects[@]}" -lm -ldl -o "$work/test_units"
"$cc" "${flags[@]}" release/extlibs/units/test/test_threads.c \
    "${objects[@]}" -lm -ldl -o "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/units/examples/units_c.c \
    "${objects[@]}" -lm -ldl -o "$work/example_units"
timeout 90s "$work/test_units"
timeout 90s "$work/test_threads"
"$work/example_units" > "$work/c.txt"
./release/xl run release/extlibs/units/examples/units_c.c > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"

timeout 90s ./release/xl run test/cases/extlib_units.xl
timeout 90s ./release/xl run release/extlibs/units/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/units/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/units/test/owned_results.xl
timeout 90s ./release/xl cgen release/extlibs/units/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in xu_bytes xu_bytes_decimal xu_duration xu_duration_short xu_count; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'units: whole-return probe must cover %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'units: adopted return text was copied again\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/units.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"

timeout 90s ./release/xl build test/cases/extlib_units.xl -o "$work/units_core"
timeout 30s "$work/units_core"
for name in boundaries allocations owned_results; do
    timeout 90s ./release/xl build "release/extlibs/units/test/$name.xl" -o "$work/units_$name"
    timeout 30s "$work/units_$name"
done
timeout 90s ./release/xl build release/examples/extlibs/units.xl -o "$work/units_xl_aot"
timeout 30s "$work/units_xl_aot" > "$work/xl_aot.txt"
diff -u "$work/c.txt" "$work/xl_aot.txt"
timeout 90s ./release/xl build release/extlibs/units/examples/units_c.c -o "$work/units_c_aot"
timeout 30s "$work/units_c_aot" > "$work/c_aot.txt"
diff -u "$work/c.txt" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'units Linux C ASan/UBSan, threads and bilingual AOT gate: PASS\n'
