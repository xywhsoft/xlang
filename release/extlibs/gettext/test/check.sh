#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-gettext-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    set +e
    cd "$root" || exit 125
    mapfile -d '' after < <(input_paths | sort -zu)
    sha256sum "${after[@]}" > "$work/inputs.after.sha256"
    if ! diff -u "$work/inputs.before.sha256" "$work/inputs.after.sha256"; then result=125; fi
    printf '%s\n' "$result" > "$work/exit-code.txt"
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[gettext] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
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
cc1_path=''
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/gettext
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

input_paths() {
    { find release/extlibs/gettext release/include -type f -print0
      printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so \
          release/libxlc.so release/include/xrt_decl.h release/include/xruntime_decl.h \
          release/include/xl_extlib_abi.h test/cases/extlib_gettext.xl \
          test/fixtures/gettext_mini.mo release/examples/extlibs/gettext.xl
      if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
      if [[ -n "$cc1_path" ]]; then printf '%s\0' "$cc1_path"; fi
    }
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
cc1_path=$("$cc" -print-prog-name=cc1)
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"

# 独立编译共同对象，边界、范例与外部目录测试不再反复编译整份 XRT。
"$cc" "${flags[@]}" -c release/extlibs/gettext/test/xrt_runtime.c -o "$work/xrt.o"
"$cc" "${flags[@]}" -c release/extlibs/gettext/gettext_native.c -o "$work/native.o"
"$cc" "${flags[@]}" -c release/extlibs/gettext/gettext_plural.c -o "$work/plural.o"
native=("$work/native.o" "$work/plural.o" "$work/xrt.o")
"$cc" "${flags[@]}" release/extlibs/gettext/test/test_gettext.c "${native[@]}" \
    -no-pie -lm -ldl -o "$work/test_gettext"
timeout 90s "$work/test_gettext" "$work"

"$cc" "${flags[@]}" release/extlibs/gettext/test/test_context_queries.c "${native[@]}" \
    -no-pie -lm -ldl -o "$work/test_context_queries"
timeout 30s "$work/test_context_queries"
"$cc" "${flags[@]}" release/extlibs/gettext/test/po_layout_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/po-layout-c"
timeout 90s "$work/po-layout-c"
timeout 90s ./release/xl run release/extlibs/gettext/test/po_layout_c.c
timeout 90s ./release/xl run release/extlibs/gettext/test/po_layout.xl
"$cc" "${flags[@]}" release/extlibs/gettext/test/po_spans_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/po-spans-c"
timeout 90s "$work/po-spans-c"
timeout 90s ./release/xl run release/extlibs/gettext/test/po_spans_c.c
timeout 90s ./release/xl run release/extlibs/gettext/test/po_spans.xl
layout_source=release/extlibs/gettext/test/po_layout.po
"$cc" "${flags[@]}" release/extlibs/gettext/examples/layout_c.c "$work/xrt.o" -no-pie -lm -ldl -o "$work/layout-c"
timeout 30s "$work/layout-c" "$layout_source" > "$work/layout-c.txt"
diff -u <(sed 's/\r$//' release/extlibs/gettext/examples/layout.expected.txt) "$work/layout-c.txt"
timeout 90s ./release/xl run release/extlibs/gettext/examples/layout_c.c -- "$layout_source" > "$work/layout-direct.txt"
timeout 90s ./release/xl run release/extlibs/gettext/examples/layout.xl -- "$layout_source" > "$work/layout-xl.txt"
diff -u "$work/layout-c.txt" "$work/layout-direct.txt"
diff -u "$work/layout-c.txt" "$work/layout-xl.txt"
timeout 90s ./release/xl cgen release/extlibs/gettext/test/boundaries.xl --emit "$work/module.c"
grep -E '^extern .*xgt_[a-z_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xgt_[a-z_]+"' release/extlibs/gettext/src/gettext.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xgt_[a-z_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 7
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#define XRT_MODULE_ALL\n#include "xrt.h"\n#include "gettext_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
"$cc" "${flags[@]}" -fsyntax-only "$work/abi.c"
timeout 90s ./release/xl run test/cases/extlib_gettext.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/duplicates.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/gettext/test/input_limits.xl

"$cc" "${flags[@]}" release/extlibs/gettext/examples/gettext_c.c "$work/xrt.o" \
    -no-pie -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/example_c.txt"
timeout 90s ./release/xl run release/extlibs/gettext/examples/gettext_c.c > "$work/example_direct_c.txt"
diff -u "$work/example_c.txt" "$work/example_direct_c.txt"
( cd release && timeout 90s ./xl run extlibs/gettext/examples/gettext.xl ) > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
( cd release && timeout 90s ./xl run examples/extlibs/gettext.xl ) > "$work/public_xl.txt"
for name in duplicates boundaries allocations input_limits; do
    timeout 120s ./release/xl build "release/extlibs/gettext/test/$name.xl" \
        -o "$work/gettext_$name"
    timeout 90s "$work/gettext_$name"
done
timeout 120s ./release/xl build test/cases/extlib_gettext.xl \
    -o "$work/gettext_central"
timeout 90s "$work/gettext_central"
( cd release && timeout 120s ./xl build extlibs/gettext/examples/gettext.xl \
    -o "$work/gettext_example" )
( cd release && timeout 90s "$work/gettext_example" ) > "$work/example_aot.txt"
diff -u "$work/example_c.txt" "$work/example_aot.txt"
timeout 120s ./release/xl build release/examples/extlibs/gettext.xl \
    -o "$work/gettext_public_example"
( cd release && timeout 90s "$work/gettext_public_example" ) > "$work/public_aot.txt"
diff -u "$work/public_xl.txt" "$work/public_aot.txt"
timeout 120s ./release/xl build release/extlibs/gettext/examples/gettext_c.c \
    -o "$work/gettext_c_example"
timeout 90s "$work/gettext_c_example" > "$work/example_c_aot.txt"
diff -u "$work/example_c.txt" "$work/example_c_aot.txt"
for source in test/po_layout.xl test/po_layout_c.c test/po_spans.xl test/po_spans_c.c examples/layout.xl examples/layout_c.c; do
    target="$work/$(basename "${source%.*}")"
    timeout 120s ./release/xl build "release/extlibs/gettext/$source" -o "$target"
    if [[ "$source" == examples/* ]]; then
        timeout 90s "$target" "$layout_source" > "$work/$(basename "$source").txt"
        diff -u "$work/layout-c.txt" "$work/$(basename "$source").txt"
    else timeout 90s "$target"; fi
done
if command -v msgfmt >/dev/null 2>&1; then
    msgfmt --check -o "$work/external.mo" release/extlibs/gettext/test/external.po
    "$cc" "${flags[@]}" release/extlibs/gettext/test/external_mo_c.c "${native[@]}" \
        -no-pie -lm -ldl -o "$work/external_c"
    timeout 90s "$work/external_c" "$work/external.mo"
    timeout 90s ./release/xl run release/extlibs/gettext/test/external_mo.xl \
        -- "$work/external.mo"
    timeout 120s ./release/xl build release/extlibs/gettext/test/external_mo.xl \
        -o "$work/external_xl"
    timeout 90s "$work/external_xl" "$work/external.mo"
    msgfmt --check -o "$work/layout.mo" "$layout_source"
    timeout 30s "$work/layout-c" "$work/layout.mo" mo > "$work/gnu-c.txt"
    timeout 90s ./release/xl run release/extlibs/gettext/examples/layout.xl -- "$work/layout.mo" mo > "$work/gnu-xl.txt"
    timeout 90s "$work/layout" "$work/layout.mo" mo > "$work/gnu-xl-aot.txt"
    timeout 90s "$work/layout_c" "$work/layout.mo" mo > "$work/gnu-c-aot.txt"
    for result in gnu-c gnu-xl gnu-xl-aot gnu-c-aot; do diff -u "$work/layout-c.txt" "$work/$result.txt"; done
else
    printf 'gettext GNU msgfmt interop: SKIP (msgfmt unavailable)\n'
fi
printf 'gettext Linux C ASan/UBSan, XLang JIT/AOT and parity gate: PASS\n'
