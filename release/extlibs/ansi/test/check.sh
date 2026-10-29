#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-ansi-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf 'exit=%s\n' "$result" > "$work.report.txt"
    cp -- "$work/inputs.before.sha256" "$work.inputs.sha256"
    if ((result == 0)); then rm -rf -- "$work" || result=$?
    else printf '[ansi] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    printf '[ansi] report: %s.report.txt\n' "$work"
    exit "$result"
}
trap cleanup EXIT
record_failure() {
    printf 'exit=%s\nline=%s\ncommand=%s\npipeline=%s\n' "$@" > "$work/failure.txt"
    cat "$work/failure.txt" >&2
}
trap 'record_failure "$?" "$LINENO" "$BASH_COMMAND" "${PIPESTATUS[*]}"' ERR
cc=${CC:-gcc}
cc_path=$(command -v "$cc" || true)
input_paths() {
    find release/extlibs/ansi release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_ansi.xl release/examples/extlibs/ansi.xl
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/ansi
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
compile() { timeout 180s "$cc" "${flags[@]}" "$@"; }
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
compile -x c -fsyntax-only release/extlibs/ansi/include/ansi_native.h
compile -DXRT_MODULE_STRING -DXRT_MODULE_STRING_FORMAT -DXRT_MODULE_UNICODE -fsyntax-only release/extlibs/ansi/ansi_native.c
compile -c release/extlibs/ansi/test/xrt_runtime.c -o "$work/runtime.o"
compile -c release/extlibs/ansi/ansi_native.c -o "$work/native.o"
objects=("$work/native.o" "$work/runtime.o")
for name in test_ansi test_threads input_budgets_c; do
    compile -DNDEBUG "release/extlibs/ansi/test/$name.c" "${objects[@]}" -no-pie -lm -ldl -o "$work/$name"
    timeout 90s "$work/$name"
done
timeout 90s ./release/xl run release/extlibs/ansi/test/input_budgets_c.c
for source in test/cases/extlib_ansi.xl release/extlibs/ansi/test/boundaries.xl \
    release/extlibs/ansi/test/allocations.xl release/extlibs/ansi/test/input_budgets.xl release/extlibs/ansi/test/owned_probe.xl; do
    timeout 90s ./release/xl run "$source"
done
timeout 90s ./release/xl cgen release/extlibs/ansi/examples/ansi.xl --emit "$work/module.c"
grep -E '^extern .*xansi_[a-z0-9_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xansi_[a-z0-9_]+"' release/extlibs/ansi/src/ansi.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xansi_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 30
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "include/ansi_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile -fsyntax-only "$work/abi.c"
grep -oE '"xansi_[a-z0-9_]+" @owned' release/extlibs/ansi/src/ansi.xl | cut -d '"' -f 2 > "$work/owned.names"
test "$(wc -l < "$work/owned.names")" -eq 29
while read -r name; do
    test "$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*$name[[:space:]]*\(" "$work/module.c")" -eq 1
done < "$work/owned.names"
timeout 90s ./release/xl cgen release/extlibs/ansi/test/owned_probe.xl --emit "$work/owned_probe.c"
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'ANSI input validation or return still copies text\n' >&2
    exit 1
fi
compile -DNDEBUG release/extlibs/ansi/examples/ansi_c.c "${objects[@]}" -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
diff -u <(sed 's/\r$//' release/extlibs/ansi/test/ansi.expected.txt) "$work/c.txt"
timeout 90s ./release/xl run release/extlibs/ansi/examples/ansi_c.c > "$work/direct.txt"
timeout 90s ./release/xl run release/extlibs/ansi/examples/ansi.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
timeout 90s ./release/xl run release/examples/extlibs/ansi.xl > "$work/public.txt"
for source in test/cases/extlib_ansi.xl release/extlibs/ansi/test/boundaries.xl \
    release/extlibs/ansi/test/allocations.xl release/extlibs/ansi/test/input_budgets.xl \
    release/extlibs/ansi/test/owned_probe.xl release/extlibs/ansi/test/input_budgets_c.c \
    release/extlibs/ansi/examples/ansi.xl release/extlibs/ansi/examples/ansi_c.c release/examples/extlibs/ansi.xl; do
    target="$work/$(basename "${source%.*}")-$(basename "$(dirname "$source")")"
    timeout 180s ./release/xl build "$source" -o "$target"
    timeout 90s "$target" > "$target.out"
    case "$source" in
        release/extlibs/ansi/examples/*) diff -u "$work/c.txt" "$target.out" ;;
        release/examples/extlibs/ansi.xl) diff -u "$work/public.txt" "$target.out" ;;
    esac
done
printf 'ansi Linux C ASan/UBSan, 30 actual prototypes, input budgets and bilingual JIT/AOT: PASS\n'
