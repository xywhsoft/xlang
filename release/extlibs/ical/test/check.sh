#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ical-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[ical] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
python=${PYTHON:-python3}
run() {
    printf '%q ' "$@" > "$work/last-command.txt"
    printf '\n' >> "$work/last-command.txt"
    timeout 180s "$@"
}
mapfile -d '' inputs < <(find release/extlibs/ical release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
    test/cases/extlib_ical.xl test/cases/extlib_ical_boundaries.xl release/examples/extlibs/ical.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run "$cc" --version > "$work/compiler.txt"
cc1=$(run "$cc" -print-prog-name=cc1)
sha256sum "$(command -v "$cc")" "$cc1" "$(command -v "$python")" >> "$work/inputs.before.sha256"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/ical/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

run "$cc" "${flags[@]}" -fsyntax-only -DXRT_MODULE_BUFFER \
    -DXRT_MODULE_UNICODE -DXRT_MODULE_VALUE_CONTAINER \
    release/extlibs/ical/ical_native.c release/extlibs/ical/ical_parse.c \
    release/extlibs/ical/ical_build.c
run "$cc" "${flags[@]}" -c release/extlibs/ical/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
for unit in ical_native ical_parse ical_build; do
    run "$cc" "${flags[@]}" -c "release/extlibs/ical/$unit.c" -o "$work/$unit.o"
done
objects=("$work/xrt_runtime.o" "$work/ical_native.o" "$work/ical_parse.o" "$work/ical_build.o")
run "$cc" "${flags[@]}" release/extlibs/ical/test/test_ical.c \
    "${objects[@]}" -lm -ldl -o "$work/test_ical"
run "$cc" "${flags[@]}" release/extlibs/ical/examples/ical.c \
    "$work/xrt_runtime.o" -lm -ldl -o "$work/example_ical"
run "$work/test_ical" "$work/long.ics"
run "$work/example_ical" "$work/c.ics" > "$work/c.txt"
run ./release/xl run release/extlibs/ical/examples/ical.c \
    > "$work/c_direct.txt"
diff -u "$work/c.txt" "$work/c_direct.txt"

run "$cc" "${flags[@]}" release/extlibs/ical/test/test_utf8_folding.c "${objects[@]}" -lm -ldl -o "$work/folding"
run "$work/folding"
run ./release/xl run release/extlibs/ical/test/test_utf8_folding.c
run ./release/xl run release/extlibs/ical/test/utf8_folding.xl
run ./release/xl cgen release/extlibs/ical/test/utf8_folding.xl --emit "$work/generated.c"
grep -E '^extern [^;]*\bxi_(parse|build)\(' "$work/generated.c" > "$work/prototypes.txt"
[[ $(wc -l < "$work/prototypes.txt") == 2 ]]
printf '#include "ical_native.h"\n' > "$work/abi.c"
cat "$work/prototypes.txt" >> "$work/abi.c"
run "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/^extern .* xi_parse(/extern int xi_parse(/' "$work/abi.c" > "$work/wrong.c"
if run "$cc" "${flags[@]}" -fsyntax-only "$work/wrong.c" > "$work/wrong.log" 2>&1; then
    printf 'wrong parse return accepted\n' >&2; exit 1
else [[ $? == 1 ]]; fi

run "$cc" "${flags[@]}" release/extlibs/ical/examples/folded_calendar_c.c "${objects[@]}" -lm -ldl -o "$work/folded_gcc"
run "$work/folded_gcc" "$work/folded_gcc.ics" > "$work/folded_gcc.out"
diff -u release/extlibs/ical/examples/folded_calendar.expected.txt "$work/folded_gcc.out"
run "$python" -I release/extlibs/ical/test/check_folded.py "$work/folded_gcc.ics"
for source in release/extlibs/ical/examples/folded_calendar_c.c release/extlibs/ical/examples/folded_calendar.xl; do
    name=$(basename "$source")
    run ./release/xl run "$source" -- "$work/$name.jit.ics" > "$work/$name.jit.out"
    diff -u release/extlibs/ical/examples/folded_calendar.expected.txt "$work/$name.jit.out"
    run ./release/xl build "$source" -o "$work/$name.exe"
    run "$work/$name.exe" "$work/$name.aot.ics" > "$work/$name.aot.out"
    diff -u release/extlibs/ical/examples/folded_calendar.expected.txt "$work/$name.aot.out"
    run "$python" -I release/extlibs/ical/test/check_folded.py "$work/$name.jit.ics" "$work/$name.aot.ics"
done
for source in release/extlibs/ical/test/test_utf8_folding.c release/extlibs/ical/test/utf8_folding.xl release/extlibs/ical/examples/ical.c; do
    name=$(basename "$source")
    run ./release/xl build "$source" -o "$work/$name.exe"
    run "$work/$name.exe"
done
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'ical focused UTF8/C/XLang/AOT checks: PASS; full gate continues\n'

# 原有语言容器仍是必须通过的正例；不得把 M1 误当作本包已签收。
run ./release/xl run test/cases/extlib_ical.xl
run ./release/xl run test/cases/extlib_ical_boundaries.xl
run ./release/xl run release/extlibs/ical/test/control_characters.xl
run ./release/xl run release/extlibs/ical/test/allocations.xl
run ./release/xl run release/extlibs/ical/test/text_identifiers.xl
run ./release/xl run release/extlibs/ical/test/nested_calendar.xl
run ./release/xl run release/extlibs/ical/test/limits.xl
run ./release/xl run release/examples/extlibs/ical.xl -- "$work/xl.ics" > "$work/xl.txt"
diff -u "$work/c.txt" "$work/xl.txt"
run "$python" release/extlibs/ical/test/check_wire.py "$work/c.ics" "$work/xl.ics" "$work/long.ics"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'ical Linux C ASan/UBSan, XLang and wire gate: PASS\n'
