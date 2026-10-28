#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-totp-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[totp] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
       -Irelease/extlibs/totp/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mapfile -d '' inputs < <(find release/extlibs/totp release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
          test/cases/extlib_totp.xl release/examples/extlibs/totp.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/totp/include -DXRT_MODULE_CRYPTO_SHA1 \
    -DXRT_MODULE_TIME release/extlibs/totp/totp_native.c

"$cc" "${flags[@]}" -c release/extlibs/totp/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/totp/totp_native.c \
    release/extlibs/totp/test/test_totp.c "$work/xrt_runtime.o" \
    -Wl,--wrap=clock_gettime -lm -ldl -o "$work/test_totp"
timeout 90s "$work/test_totp"
"$cc" "${flags[@]}" release/extlibs/totp/totp_native.c \
    release/extlibs/totp/test/test_threads.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_threads"
timeout 90s "$work/test_threads"
"$cc" "${flags[@]}" release/extlibs/totp/totp_native.c \
    release/extlibs/totp/examples/totp_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/totp_c"
timeout 30s "$work/totp_c" > "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/totp/examples/totp_c.c > "$work/c_direct.txt"
diff -u "$work/c_example.txt" "$work/c_direct.txt"
diff -u <(printf 'HOTP(0): 755224\nTOTP(59): 94287082\nvalid: true\nlive valid: true\n') \
    "$work/c_example.txt"

timeout 45s ./release/xl run test/cases/extlib_totp.xl
timeout 45s ./release/xl run release/extlibs/totp/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/totp/test/allocations.xl
timeout 45s ./release/xl run release/extlibs/totp/test/owned_results.xl
timeout 45s ./release/xl cgen release/extlibs/totp/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in generate generate_at hotp; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xtotp_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'totp: return probe must cover %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'totp: adopted text was copied again\n' >&2
    exit 1
fi
timeout 45s ./release/xl run release/examples/extlibs/totp.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
timeout 120s ./release/xl build test/cases/extlib_totp.xl \
    -o "$work/totp_core_aot"
timeout 45s "$work/totp_core_aot"
for name in boundaries allocations owned_results; do
    timeout 120s ./release/xl build "release/extlibs/totp/test/$name.xl" \
        -o "$work/totp_${name}_aot"
    timeout 45s "$work/totp_${name}_aot"
done
timeout 120s ./release/xl build release/examples/extlibs/totp.xl \
    -o "$work/totp_xl_example_aot"
timeout 45s "$work/totp_xl_example_aot" > "$work/xl_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_aot.txt"
timeout 120s ./release/xl build release/extlibs/totp/examples/totp_c.c \
    -o "$work/totp_c_example_aot"
timeout 45s "$work/totp_c_example_aot" > "$work/c_aot.txt"
diff -u "$work/c_example.txt" "$work/c_aot.txt"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'totp Linux C ASan/UBSan and XLang parity gate: PASS\n'
