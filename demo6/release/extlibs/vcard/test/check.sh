#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-vcard-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/vcard/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/vcard/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/vcard/vcard_native.c \
    release/extlibs/vcard/vcard_parse.c \
    release/extlibs/vcard/vcard_build.c \
    release/extlibs/vcard/test/test_vcard.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/test_vcard"
timeout 90s "$work/test_vcard"

xl=./release/xl
if [[ ! -x "$xl" ]]; then
    printf 'missing Linux xlang runner: %s\n' "$xl" >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_vcard.xl
timeout 90s "$xl" run test/cases/extlib_vcard_boundaries.xl
timeout 90s "$xl" run release/extlibs/vcard/test/control_characters.xl
timeout 90s "$xl" run release/extlibs/vcard/test/allocations.xl
timeout 90s "$xl" run release/extlibs/vcard/test/uri_values.xl
timeout 90s "$xl" run release/extlibs/vcard/test/structure.xl

"$cc" "${flags[@]}" release/extlibs/vcard/vcard_native.c \
    release/extlibs/vcard/vcard_parse.c \
    release/extlibs/vcard/vcard_build.c \
    release/extlibs/vcard/examples/vcard.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/vcard_example"
timeout 90s "$work/vcard_example" "$work/c.vcf" > "$work/c.out"
set +e
timeout 90s "$work/vcard_example" "$work" > "$work/write_failure.out" \
    2> "$work/write_failure.err"
write_status=$?
set -e
if [[ $write_status != 1 || -s "$work/write_failure.out" ||
      -s "$work/write_failure.err" ]]; then
    cat "$work/write_failure.err" >&2
    printf 'vcard C example failed-write cleanup: FAIL\n' >&2
    exit 1
fi
timeout 90s "$xl" run release/extlibs/vcard/examples/vcard.c -- \
    "$work/direct.vcf" > "$work/direct.out"
diff -u "$work/c.out" "$work/direct.out"
cmp "$work/c.vcf" "$work/direct.vcf"
timeout 90s "$xl" run release/examples/extlibs/vcard.xl -- \
    "$work/xlang.vcf" > "$work/xlang.out"
diff -u "$work/c.out" "$work/xlang.out"
cmp "$work/c.vcf" "$work/xlang.vcf"
printf 'vcard Linux C ASan/UBSan, XLang JIT and bilingual gate: PASS\n'
