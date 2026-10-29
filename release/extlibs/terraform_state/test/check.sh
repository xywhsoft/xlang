#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-terraform-state-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/terraform_state/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only \
    release/extlibs/terraform_state/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/terraform_state/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" \
    release/extlibs/terraform_state/terraform_state_native.c \
    release/extlibs/terraform_state/test/test_terraform_state.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_terraform_state"
timeout 90s "$work/test_terraform_state"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_terraform_state.xl
timeout 90s "$xl" run release/extlibs/terraform_state/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/terraform_state/test/allocations.xl

"$cc" "${flags[@]}" \
    release/extlibs/terraform_state/terraform_state_native.c \
    release/extlibs/terraform_state/examples/basic_c.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example"
timeout 90s "$work/example" > "$work/c.out"
diff -u release/extlibs/terraform_state/test/basic.expected.txt "$work/c.out"
timeout 90s "$xl" run release/extlibs/terraform_state/examples/basic_c.c > "$work/direct_c.out"
timeout 90s "$xl" run release/extlibs/terraform_state/examples/basic.xl \
    > "$work/xlang.out"
diff -u "$work/c.out" "$work/direct_c.out"
diff -u "$work/c.out" "$work/xlang.out"
printf 'terraform_state Linux C ASan/UBSan, XLang JIT and bilingual gate: PASS\n'
