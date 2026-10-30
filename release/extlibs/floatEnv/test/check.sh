#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-floatenv-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -DXRT_MODULE_THREAD -Ilib/xrt -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/floatEnv/test/xrt_runtime.c \
    -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/floatEnv/test/test_float_env.c \
    -o "$work/test.o"
"$cc" "${flags[@]}" -c release/extlibs/floatEnv/float_env_native.c \
    -o "$work/float_env.o"
"$cc" -fsanitize=address,undefined -no-pie "$work/test.o" "$work/float_env.o" \
    "$work/runtime.o" \
    -pthread -lm -ldl -o "$work/test"
timeout 90s "$work/test"
"$cc" "${flags[@]}" release/extlibs/floatEnv/examples/float_env_c.c \
    "$work/float_env.o" "$work/runtime.o" -no-pie -lm -ldl \
    -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"

if [[ ! -x ./release/xl ]]; then
    printf 'floatEnv: Linux release/xl is missing\n' >&2
    exit 1
fi
timeout 45s ./release/xl run release/extlibs/floatEnv/examples/float_env_c.c \
    > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"
timeout 45s ./release/xl run release/extlibs/floatEnv/test/boundaries.xl
for method in restore hold; do
    if timeout 45s ./release/xl check \
        "release/extlibs/floatEnv/test/private_${method}_native_negative.xl" \
        > "$work/private.txt" 2>&1; then
        printf 'floatEnv: %sNative is accessible outside Snapshot\n' "$method" >&2
        exit 1
    else
        status=$?
        if [[ $status -ne 1 ]] || ! grep -q "${method}Native" "$work/private.txt" ||
            ! grep -q 'access' "$work/private.txt"; then
            cat "$work/private.txt" >&2
            exit 1
        fi
    fi
done
if timeout 45s ./release/xl check \
    release/extlibs/floatEnv/test/private_module_native_negative.xl \
    > "$work/module_native.txt" 2>&1; then
    printf 'floatEnv: module-native handle operation is exported\n' >&2
    exit 1
else
    status=$?
    if [[ $status -ne 1 ]] || ! grep -q 'no such exported symbol' "$work/module_native.txt"; then
        cat "$work/module_native.txt" >&2
        exit 1
    fi
fi
if timeout 45s ./release/xl check \
    release/extlibs/floatEnv/test/private_round_native_negative.xl \
    > "$work/round_native.txt" 2>&1; then
    printf 'floatEnv: round-native operation is exported\n' >&2
    exit 1
else
    status=$?
    if [[ $status -ne 1 ]] || ! grep -q 'no such exported symbol' "$work/round_native.txt"; then
        cat "$work/round_native.txt" >&2
        exit 1
    fi
fi
timeout 45s ./release/xl run release/extlibs/floatEnv/test/allocations.xl
timeout 45s ./release/xl run release/examples/extlibs/floatEnv.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
if timeout 45s ./release/xl check \
    release/extlibs/floatEnv/test/private_adopt_constructor_negative.xl \
    > "$work/private_constructor.txt" 2>&1; then
    printf 'floatEnv: private Snapshot constructor is accessible outside the package\n' >&2
    exit 1
else
    status=$?
    if [[ $status -ne 1 ]] || ! grep -q 'expected 0, actual 1' "$work/private_constructor.txt"; then
        cat "$work/private_constructor.txt" >&2
        exit 1
    fi
fi
timeout 90s ./release/xl build test/cases/stdlib_crt_float_env.xl \
    -o "$work/core"
timeout 45s "$work/core"
timeout 90s ./release/xl build release/extlibs/floatEnv/test/boundaries.xl \
    -o "$work/boundaries"
timeout 45s "$work/boundaries"
timeout 90s ./release/xl build release/examples/extlibs/floatEnv.xl \
    -o "$work/xlang_example"
timeout 45s "$work/xlang_example" > "$work/xlang_aot.txt"
diff -u "$work/c.txt" "$work/xlang_aot.txt"
printf 'floatEnv Linux C ASan/UBSan and XLang parity gate: PASS\n'
