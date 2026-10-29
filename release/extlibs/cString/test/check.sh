#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-cstring-XXXXXX")
cc=${CC:-gcc}
input_paths() {
    find release/extlibs/cString release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        dev/extlib_blockers/native_c_scalar_interpolation.xl "$(command -v "$cc")"
}
finish_gate() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then rm -rf -- "$work"
    else printf '[cString] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -O1 -g -Wall -Wextra -Werror
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

printf '[cString] C sanitizer gate\n'
"$cc" "${flags[@]}" -DNDEBUG release/extlibs/cString/test/test_cstring.c -o "$work/test_cstring"
timeout 30s "$work/test_cstring"
"$cc" "${flags[@]}" release/extlibs/cString/examples/cString_c.c -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
diff -u <(sed 's/\r$//' release/extlibs/cString/examples/cString.expected.txt) "$work/c.txt"
"$cc" "${flags[@]}" release/extlibs/cString/examples/records_c.c -o "$work/records-c"
timeout 30s "$work/records-c" > "$work/records-c.txt"
diff -u <(sed 's/\r$//' release/extlibs/cString/examples/records.expected.txt) "$work/records-c.txt"
timeout 45s ./release/xl run release/extlibs/cString/examples/records_c.c > "$work/records-direct.txt"
diff -u "$work/records-c.txt" "$work/records-direct.txt"
printf '[cString] XLang JIT gate\n'
timeout 45s ./release/xl run release/extlibs/cString/examples/cString_c.c > "$work/direct.txt"
timeout 45s ./release/xl run release/extlibs/cString/test/boundaries.xl
timeout 45s ./release/xl run release/extlibs/cString/test/interop.xl
timeout 45s ./release/xl run release/extlibs/cString/examples/cString.xl > "$work/xl.txt"
diff -u "$work/c.txt" "$work/direct.txt"
diff -u "$work/c.txt" "$work/xl.txt"
timeout 45s ./release/xl cgen release/extlibs/cString/test/interop.xl --emit "$work/module.c"
grep -E '^extern .*strlen[(]' "$work/module.c" | sort -u > "$work/abi.decls"
test "$(wc -l < "$work/abi.decls")" -eq 1
printf '#include <string.h>\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
printf '[cString] XLang AOT gate\n'
timeout 120s ./release/xl build release/extlibs/cString/test/boundaries.xl \
    -o "$work/cstring_boundaries"
timeout 45s "$work/cstring_boundaries"
timeout 120s ./release/xl build release/extlibs/cString/examples/cString.xl \
    -o "$work/cstring_example"
timeout 45s "$work/cstring_example" > "$work/aot.txt"
diff -u "$work/c.txt" "$work/aot.txt"
timeout 120s ./release/xl build release/extlibs/cString/examples/cString_c.c \
    -o "$work/cstring_c_example"
timeout 45s "$work/cstring_c_example" > "$work/c_aot.txt"
diff -u "$work/c.txt" "$work/c_aot.txt"
timeout 120s ./release/xl build release/extlibs/cString/test/interop.xl -o "$work/interop"
timeout 45s "$work/interop"
timeout 120s ./release/xl build release/extlibs/cString/examples/records_c.c -o "$work/records-c-aot"
timeout 45s "$work/records-c-aot" > "$work/records-c-aot.txt"
diff -u "$work/records-c.txt" "$work/records-c-aot.txt"
# 这是有效正例，不把主线缺失的 c_size 插值转换为预期失败或强转绕过。
timeout 45s ./release/xl run release/extlibs/cString/examples/records.xl > "$work/records-xl.txt"
diff -u "$work/records-c.txt" "$work/records-xl.txt"
timeout 120s ./release/xl build release/extlibs/cString/examples/records.xl -o "$work/records-xl"
timeout 45s "$work/records-xl" > "$work/records-xl-aot.txt"
diff -u "$work/records-c.txt" "$work/records-xl-aot.txt"
printf 'cString Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
