#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-css-XXXXXX")
cc=${CC:-gcc}
input_paths() {
    find release/extlibs/css release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_css.xl release/examples/extlibs/css.xl release/examples/extlibs/css_c.c "$(command -v "$cc")"
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
    else printf '[css] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2; fi
    exit "$result"
}
trap finish_gate EXIT
trap 'printf "exit=%s\nline=%s\ncommand=%s\n" "$?" "$LINENO" "$BASH_COMMAND" > "$work/failure.txt"' ERR
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt -Irelease/extlibs/css/include
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -fsyntax-only release/extlibs/css/test/abi_contract.c
"$cc" "${flags[@]}" -c release/extlibs/css/test/runtime.c -o "$work/runtime.o"
"$cc" "${flags[@]}" -c release/extlibs/css/css_native.c -o "$work/css_native.o"
for source in test_css test_css_threads; do
    "$cc" "${flags[@]}" "release/extlibs/css/test/$source.c" \
        "$work/runtime.o" "$work/css_native.o" -no-pie -lm -ldl \
        -o "$work/$source"
    timeout 90s "$work/$source"
done
"$cc" "${flags[@]}" release/extlibs/css/test/scanner_edges_c.c "$work/runtime.o" -no-pie -lm -ldl -o "$work/scanner-c"
timeout 90s "$work/scanner-c"
timeout 90s release/xl run release/extlibs/css/test/scanner_edges_c.c
"$cc" "${flags[@]}" release/extlibs/css/examples/audit_c.c "$work/runtime.o" -no-pie -lm -ldl -o "$work/audit-c"
timeout 30s "$work/audit-c" > "$work/audit-c.txt"
diff -u <(sed 's/\r$//' release/extlibs/css/examples/audit.expected.txt) "$work/audit-c.txt"
timeout 90s release/xl run release/extlibs/css/examples/audit_c.c > "$work/audit-direct-c.txt"
diff -u "$work/audit-c.txt" "$work/audit-direct-c.txt"
"$cc" "${flags[@]}" release/examples/extlibs/css_c.c \
    "$work/runtime.o" -no-pie -lm -ldl \
    -o "$work/css_example"
timeout 30s "$work/css_example" > "$work/example_c.txt"
timeout 30s release/xl run release/examples/extlibs/css_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for CSS XLang gate\n' >&2
    exit 1
fi
(
    cd release
    timeout 90s ./xl run ../test/cases/extlib_css.xl
    timeout 90s ./xl run extlibs/css/test/boundaries.xl
    timeout 90s ./xl run extlibs/css/test/allocations.xl
    timeout 90s ./xl run extlibs/css/test/scanner_edges.xl
    timeout 90s ./xl run extlibs/css/examples/audit.xl > "$work/audit-xl.txt"
    timeout 90s ./xl run examples/extlibs/css.xl > "$work/example_xl.txt"
)
diff -u "$work/example_c.txt" "$work/example_xl.txt"
diff -u "$work/audit-c.txt" "$work/audit-xl.txt"
timeout 90s release/xl cgen release/extlibs/css/test/boundaries.xl --emit "$work/module.c"
grep -E '^extern .*xcss_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xcss_[a-z_]+"' release/extlibs/css/src/css.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xcss_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 6
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "css_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
for name in get selector_at strip_comments; do
    grep -Eq "[.]Data[[:space:]]*=[[:space:]]*xcss_$name[[:space:]]*[(]" "$work/module.c"
done
for name in aot_core boundaries allocations; do
    timeout 240s release/xl build "release/extlibs/css/test/$name.xl" \
        -o "$work/css_$name"
    timeout 90s "$work/css_$name"
done
timeout 240s release/xl build release/examples/extlibs/css.xl \
    -o "$work/css_example_xl"
timeout 45s "$work/css_example_xl" > "$work/example_aot.txt"
diff -u "$work/example_c.txt" "$work/example_aot.txt"
timeout 240s release/xl build release/examples/extlibs/css_c.c \
    -o "$work/css_example_c_aot"
timeout 45s "$work/css_example_c_aot" > "$work/example_c_aot.txt"
diff -u "$work/example_c.txt" "$work/example_c_aot.txt"
for source in test/scanner_edges.xl test/scanner_edges_c.c examples/audit.xl examples/audit_c.c; do
    binary="$work/$(basename "${source%.*}")"
    timeout 240s release/xl build "release/extlibs/css/$source" -o "$binary"
    timeout 90s "$binary" > "$work/$(basename "$source").txt"
    if [[ "$source" == examples/* ]]; then diff -u "$work/audit-c.txt" "$work/$(basename "$source").txt"; fi
done
printf 'css Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
