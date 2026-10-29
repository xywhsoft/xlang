#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ini-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/ini -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

printf '[ini] C sanitizer gate\n'
"$cc" "${flags[@]}" release/extlibs/ini/test/test_ini.c \
    release/extlibs/ini/test/xrt_runtime.c -lm -ldl -o "$work/test_ini"
timeout 90s "$work/test_ini"
"$cc" "${flags[@]}" release/examples/extlibs/ini_c.c \
    release/extlibs/ini/test/xrt_runtime.c -lm -ldl -o "$work/ini_c"
timeout 90s "$work/ini_c" > "$work/gcc_example.txt"
printf '[ini] C/Python interoperability gate\n'
"$cc" "${flags[@]}" release/extlibs/ini/test/interop.c \
    release/extlibs/ini/test/xrt_runtime.c -lm -ldl -o "$work/ini_interop"
timeout 90s python3 release/extlibs/ini/test/interop.py "$work/ini_interop"
printf '[ini] XLang JIT gate\n'
timeout 90s ./release/xl run release/extlibs/ini/test/test_ini.c
timeout 90s ./release/xl run test/cases/extlib_ini.xl
timeout 90s ./release/xl run release/extlibs/ini/test/boundaries.xl
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
for name in aot_float_core boundaries; do
    timeout 180s ./release/xl build "release/extlibs/ini/test/$name.xl" \
        -o "$work/ini_$name"
    timeout 90s "$work/ini_$name"
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
printf 'ini Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
