#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-hll-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/hyperloglog/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt \
    -Irelease/extlibs/hyperloglog/include -DXRT_MODULE_HASH64 \
    release/extlibs/hyperloglog/hyperloglog_native.c
"$cc" "${flags[@]}" -fsyntax-only release/extlibs/hyperloglog/test/abi_contract.c

"$cc" "${flags[@]}" -c release/extlibs/hyperloglog/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for name in test_hyperloglog test_threads; do
    "$cc" "${flags[@]}" release/extlibs/hyperloglog/hyperloglog_native.c \
        "release/extlibs/hyperloglog/test/$name.c" "$work/xrt_runtime.o" \
        -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done

"$cc" "${flags[@]}" release/extlibs/hyperloglog/hyperloglog_native.c \
    release/extlibs/hyperloglog/examples/hyperloglog_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/hll_example"
timeout 90s "$work/hll_example" > "$work/hll_c.txt"
diff -u release/extlibs/hyperloglog/test/hyperloglog.expected.txt \
    "$work/hll_c.txt"
timeout 90s release/xl run \
    release/extlibs/hyperloglog/examples/hyperloglog_c.c \
    > "$work/hll_direct.txt"
diff -u "$work/hll_c.txt" "$work/hll_direct.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'hyperloglog Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_hyperloglog.xl
timeout 90s "$xl" run release/extlibs/hyperloglog/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/hyperloglog/test/named_type.xl
timeout 90s "$xl" run release/extlibs/hyperloglog/test/allocations.xl
for method in add merge; do
    case="private_${method}_negative"
    if timeout 90s "$xl" check "release/extlibs/hyperloglog/test/$case.xl" \
        > "$work/$case.txt" 2>&1; then
        printf 'hyperloglog: %s must be rejected\n' "$case" >&2
        exit 1
    fi
    if ! grep -Fq "${method}Native" "$work/$case.txt" ||
       ! grep -Fq 'access' "$work/$case.txt"; then
        cat "$work/$case.txt" >&2
        printf 'hyperloglog: expected private-access diagnostic\n' >&2
        exit 1
    fi
done
timeout 90s "$xl" run release/examples/extlibs/hyperloglog.xl > "$work/hll_xl.txt"
diff -u "$work/hll_c.txt" "$work/hll_xl.txt"
timeout 120s "$xl" build test/cases/extlib_hyperloglog.xl \
    -o "$work/hll_core_aot"
timeout 45s "$work/hll_core_aot"
for name in boundaries named_type allocations; do
    timeout 120s "$xl" build "release/extlibs/hyperloglog/test/$name.xl" \
        -o "$work/hll_${name}_aot"
    timeout 45s "$work/hll_${name}_aot"
done
timeout 120s "$xl" build release/examples/extlibs/hyperloglog.xl \
    -o "$work/hll_xl_example_aot"
timeout 45s "$work/hll_xl_example_aot" > "$work/hll_xl_aot.txt"
diff -u "$work/hll_c.txt" "$work/hll_xl_aot.txt"
timeout 120s "$xl" build release/extlibs/hyperloglog/examples/hyperloglog_c.c \
    -o "$work/hll_c_example_aot"
timeout 45s "$work/hll_c_example_aot" > "$work/hll_c_aot.txt"
diff -u "$work/hll_c.txt" "$work/hll_c_aot.txt"
printf 'hyperloglog Linux C/XL ASan/UBSan gate: PASS\n'
