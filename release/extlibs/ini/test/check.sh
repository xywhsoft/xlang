#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ini-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[ini] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
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
       -Ilib/xrt -Irelease/extlibs/ini -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

mapfile -d '' inputs < <(
    { find release/extlibs/ini -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/cases/extlib_ini.xl \
          release/examples/extlibs/ini.xl release/examples/extlibs/ini_c.c
    } | sort -zu
)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
printf '[ini] C sanitizer gate\n'
"$cc" "${flags[@]}" -c release/extlibs/ini/test/xrt_runtime.c -o "$work/xrt.o"
for name in test_ini test_updates test_output_limits test_interpolation_refs test_owned_text test_stream_errors; do
    "$cc" "${flags[@]}" "release/extlibs/ini/test/$name.c" \
        "$work/xrt.o" -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
"$cc" "${flags[@]}" release/examples/extlibs/ini_c.c \
    "$work/xrt.o" -lm -ldl -o "$work/ini_c"
timeout 90s "$work/ini_c" > "$work/gcc_example.txt"
"$cc" "${flags[@]}" release/extlibs/ini/examples/profiles_c.c \
    "$work/xrt.o" -lm -ldl -o "$work/profiles_c"
timeout 90s "$work/profiles_c" > "$work/profiles_gcc.txt"
printf '[ini] C/Python interoperability gate\n'
"$cc" "${flags[@]}" release/extlibs/ini/test/interop.c \
    "$work/xrt.o" -lm -ldl -o "$work/ini_interop"
timeout 90s python3 release/extlibs/ini/test/interop.py "$work/ini_interop"
printf '[ini] XLang JIT gate\n'
timeout 90s ./release/xl run release/extlibs/ini/test/test_ini.c
timeout 90s ./release/xl run release/extlibs/ini/test/test_updates.c
timeout 90s ./release/xl run release/extlibs/ini/test/test_output_limits.c
timeout 90s ./release/xl run release/extlibs/ini/test/test_interpolation_refs.c
timeout 90s ./release/xl run release/extlibs/ini/test/test_owned_text.c
timeout 90s ./release/xl run release/extlibs/ini/test/test_stream_errors.c
timeout 90s ./release/xl run test/cases/extlib_ini.xl
timeout 90s ./release/xl run release/extlibs/ini/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/ini/test/updates.xl
timeout 90s ./release/xl run release/extlibs/ini/test/output_limits.xl
timeout 90s ./release/xl run release/extlibs/ini/test/interpolation_refs.xl
timeout 90s ./release/xl run release/extlibs/ini/test/owned_results.xl
timeout 90s ./release/xl run release/extlibs/ini/test/owned_probe.xl
timeout 90s ./release/xl cgen release/extlibs/ini/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in error_text section_name key_name get stringify_text; do
    if ! grep -Eq "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xini_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c"; then
        printf 'INI probe did not adopt %s\n' "$symbol" >&2
        exit 1
    fi
done
copies=$(grep -Ec '= *xlir_string_copy *\(' "$work/owned_probe.c" || true)
if ((copies > 1)); then
    printf 'INI copied an owned result in the complete return chain\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/extlibs/ini/examples/profiles_c.c > "$work/profiles_direct_c.txt"
timeout 90s ./release/xl run release/extlibs/ini/examples/profiles.xl > "$work/profiles_xl.txt"
diff -u "$work/profiles_gcc.txt" "$work/profiles_direct_c.txt"
diff -u "$work/profiles_gcc.txt" "$work/profiles_xl.txt"
if timeout 90s ./release/xl check release/extlibs/ini/test/private_native.xl > "$work/private_native.txt" 2>&1; then
    printf 'ini: native methods must not be public\n' >&2
    exit 1
fi
if ! grep -Fq 'getNative' "$work/private_native.txt" ||
   ! grep -Fq 'access' "$work/private_native.txt"; then
    cat "$work/private_native.txt" >&2
    printf 'ini: expected private-access diagnostic\n' >&2
    exit 1
fi
if timeout 90s ./release/xl check release/extlibs/ini/test/private_config_constructor.xl > "$work/private_config_constructor.txt" 2>&1; then
    printf 'ini: raw Config constructor must not be exported\n' >&2
    exit 1
fi
if ! grep -Fq 'no such exported type' "$work/private_config_constructor.txt"; then
    cat "$work/private_config_constructor.txt" >&2
    printf 'ini: expected non-exported Config diagnostic\n' >&2
    exit 1
fi
timeout 90s ./release/xl run release/examples/extlibs/ini_c.c \
    > "$work/c_example.txt"
timeout 90s ./release/xl run release/examples/extlibs/ini.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"
diff -u "$work/gcc_example.txt" "$work/c_example.txt"
printf '[ini] XLang AOT gate\n'
for name in aot_float_core boundaries updates output_limits interpolation_refs owned_results owned_probe; do
    timeout 180s ./release/xl build "release/extlibs/ini/test/$name.xl" \
        -o "$work/ini_$name"
    timeout 90s "$work/ini_$name"
done
for name in test_owned_text test_stream_errors; do
    timeout 180s ./release/xl build "release/extlibs/ini/test/$name.c" -o "$work/$name.c.aot"
    timeout 90s "$work/$name.c.aot"
done
timeout 180s ./release/xl build release/examples/extlibs/ini.xl \
    -o "$work/ini_example"
timeout 90s "$work/ini_example" > "$work/aot_example.txt"
diff -u "$work/gcc_example.txt" "$work/aot_example.txt"
timeout 180s ./release/xl build test/cases/extlib_ini.xl \
    -o "$work/ini_shared"
timeout 90s "$work/ini_shared"
timeout 180s ./release/xl build release/examples/extlibs/ini_c.c \
    -o "$work/ini_c_aot"
timeout 90s "$work/ini_c_aot" > "$work/aot_c_example.txt"
diff -u "$work/gcc_example.txt" "$work/aot_c_example.txt"
for source in profiles.xl profiles_c.c; do
    target="$work/${source//./_}"
    timeout 180s ./release/xl build "release/extlibs/ini/examples/$source" -o "$target"
    timeout 90s "$target" > "$work/${source//./_}.txt"
    diff -u "$work/profiles_gcc.txt" "$work/${source//./_}.txt"
done
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.checked.txt"
printf 'ini Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
