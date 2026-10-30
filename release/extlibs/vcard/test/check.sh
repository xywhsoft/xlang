#!/usr/bin/env bash
set -Eeuo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-vcard-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[vcard] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    if [[ ! -s "$work/failure.txt" ]]; then
        printf 'exit=%s\nline=%s\ncommand=%s\n' "$@" > "$work/failure.txt"
    fi
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND"' ERR
exec > >(tee "$work/output.log") 2>&1
cc=${CC:-gcc}
python=${PYTHON:-python3}
export LC_ALL=C
run() {
    printf '%q ' "$@" > "$work/last-command.txt"
    printf '\n' >> "$work/last-command.txt"
    timeout 180s "$@"
}
mapfile -d '' inputs < <(find release/extlibs/vcard release/include -type f -print0)
inputs+=(lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so
    test/cases/extlib_vcard.xl test/cases/extlib_vcard_boundaries.xl release/examples/extlibs/vcard.xl)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
run "$cc" --version > "$work/compiler.txt"
cc1=$(run "$cc" -print-prog-name=cc1)
sha256sum "$(command -v "$cc")" "$cc1" "$(command -v "$python")" >> "$work/inputs.before.sha256"
run "$python" --version > "$work/python.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -Irelease/extlibs/vcard/include -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
run "$cc" "${flags[@]}" -fsyntax-only -DXRT_MODULE_BUFFER -DXRT_MODULE_UNICODE \
    -DXRT_MODULE_VALUE_CONTAINER release/extlibs/vcard/vcard_native.c \
    release/extlibs/vcard/vcard_parse.c release/extlibs/vcard/vcard_build.c
run "$cc" "${flags[@]}" -c release/extlibs/vcard/test/xrt_runtime.c -o "$work/runtime.o"
objects=("$work/runtime.o")
for unit in vcard_native vcard_parse vcard_build; do
    run "$cc" "${flags[@]}" -c "release/extlibs/vcard/$unit.c" -o "$work/$unit.o"
    objects+=("$work/$unit.o")
done
for name in test_vcard test_utf8_folding; do
    run "$cc" "${flags[@]}" "release/extlibs/vcard/test/$name.c" "${objects[@]}" -lm -ldl -o "$work/$name"
    args=()
    if [[ "$name" == test_vcard ]]; then args=("$work/long.vcf" "$work/four.vcf"); fi
    run "$work/$name" "${args[@]}"
done
xl=./release/xl
run "$xl" run release/extlibs/vcard/test/test_utf8_folding.c
run "$xl" run release/extlibs/vcard/test/utf8_folding.xl
run "$xl" cgen release/extlibs/vcard/test/utf8_folding.xl --emit "$work/generated.c"
grep -E '^extern [^;]*\bxv_(parse|build)\(' "$work/generated.c" > "$work/prototypes.txt"
[[ $(wc -l < "$work/prototypes.txt") == 2 ]]
printf '#include "vcard_native.h"\n' > "$work/abi.c"
cat "$work/prototypes.txt" >> "$work/abi.c"
run "$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
sed 's/^extern .* xv_parse(/extern int xv_parse(/' "$work/abi.c" > "$work/wrong.c"
if run "$cc" "${flags[@]}" -fsyntax-only "$work/wrong.c" > "$work/wrong.log" 2>&1; then
    printf 'wrong parse return accepted\n' >&2; exit 1
else [[ $? == 1 ]]; fi
grep -q 'conflicting types' "$work/wrong.log"

run "$cc" "${flags[@]}" release/extlibs/vcard/examples/vcard.c "${objects[@]}" -lm -ldl -o "$work/original"
run "$work/original" "$work/c.vcf" > "$work/c.out"
run "$xl" run release/extlibs/vcard/examples/vcard.c -- "$work/direct.vcf" > "$work/direct.out"
diff -u "$work/c.out" "$work/direct.out"
cmp "$work/c.vcf" "$work/direct.vcf"
for engine in GCC JIT; do
    if [[ "$engine" == GCC ]]; then command=("$work/original")
    else command=("$xl" run release/extlibs/vcard/examples/vcard.c --); fi
    if run "${command[@]}" "$work" > "$work/failed-write.out" 2> "$work/failed-write.err"; then
        printf 'failed write returned success\n' >&2; exit 1
    else [[ $? == 1 ]]; fi
    [[ ! -s "$work/failed-write.out" && ! -s "$work/failed-write.err" ]]
done

run "$cc" "${flags[@]}" release/extlibs/vcard/examples/folded_contact_c.c "${objects[@]}" -lm -ldl -o "$work/folded_gcc"
run "$work/folded_gcc" "$work/folded_gcc.vcf" > "$work/folded_gcc.out"
diff -u release/extlibs/vcard/examples/folded_contact.expected.txt "$work/folded_gcc.out"
run "$python" -I release/extlibs/vcard/test/check_folded.py "$work/folded_gcc.vcf"
for source in release/extlibs/vcard/examples/folded_contact_c.c release/extlibs/vcard/examples/folded_contact.xl; do
    name=$(basename "$source")
    run "$xl" run "$source" -- "$work/$name.jit.vcf" > "$work/$name.jit.out"
    diff -u release/extlibs/vcard/examples/folded_contact.expected.txt "$work/$name.jit.out"
    run "$xl" build "$source" -o "$work/$name.exe"
    run "$work/$name.exe" "$work/$name.aot.vcf" > "$work/$name.aot.out"
    diff -u release/extlibs/vcard/examples/folded_contact.expected.txt "$work/$name.aot.out"
    run "$python" -I release/extlibs/vcard/test/check_folded.py "$work/$name.jit.vcf" "$work/$name.aot.vcf"
done
for source in release/extlibs/vcard/test/test_utf8_folding.c release/extlibs/vcard/test/utf8_folding.xl; do
    name=$(basename "$source")
    run "$xl" build "$source" -o "$work/$name.exe"
    run "$work/$name.exe"
done
run "$xl" build release/extlibs/vcard/examples/vcard.c -o "$work/original_c"
run "$work/original_c" "$work/aot-c.vcf" > "$work/aot-c.out"
diff -u "$work/c.out" "$work/aot-c.out"
cmp "$work/c.vcf" "$work/aot-c.vcf"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'vcard focused UTF8/C/XLang/AOT checks: PASS; full gate continues\n'

# 原字典创建保持为必须通过的正例；不改写成原生 parse 对象来规避 M1。
for source in test/cases/extlib_vcard.xl test/cases/extlib_vcard_boundaries.xl \
    release/extlibs/vcard/test/control_characters.xl release/extlibs/vcard/test/allocations.xl \
    release/extlibs/vcard/test/uri_values.xl release/extlibs/vcard/test/structure.xl \
    release/extlibs/vcard/test/limits.xl; do
    run "$xl" run "$source"
done
run "$xl" run release/examples/extlibs/vcard.xl -- "$work/xlang.vcf" > "$work/xlang.out"
diff -u "$work/c.out" "$work/xlang.out"
cmp "$work/c.vcf" "$work/xlang.vcf"
run "$xl" build release/extlibs/vcard/test/aot_core.xl -o "$work/core"
run "$work/core"
run "$xl" build release/examples/extlibs/vcard.xl -o "$work/original_xlang"
run "$work/original_xlang" "$work/aot.vcf" > "$work/aot.out"
diff -u "$work/c.out" "$work/aot.out"
cmp "$work/c.vcf" "$work/aot.vcf"
sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'vcard Linux C ASan/UBSan, XLang JIT and bilingual AOT gate: PASS\n'
