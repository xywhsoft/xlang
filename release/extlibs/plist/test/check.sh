#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-plist-XXXXXX")
cleanup() {
    result=$?
    trap - EXIT
    if [ "$result" -eq 0 ]; then
        rm -rf -- "$work" || result=$?
    else
        printf '[plist] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
        if [ -f "$work/last-command.txt" ]; then cat "$work/last-command.txt" >&2; fi
    fi
    exit "$result"
}
trap cleanup EXIT
run() {
    printf '%s\n' "$*" > "$work/last-command.txt"
    "$@"
}
cd "$root"

pkg=release/extlibs/plist
flags='-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread -Ilib/xrt -Irelease/extlibs/plist/include'
links='-fsanitize=address,undefined -pthread -ldl -lm'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
find release/extlibs/plist release/extlibs/xml.dom release/extlibs/xml.sax \
    release/extlibs/_expat release/include -type f -print0 |
    xargs -0 sha256sum > "$work/inputs.before.sha256"
sha256sum lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
    test/cases/extlib_plist.xl release/examples/extlibs/plist.xl release/examples/extlibs/plist_c.c \
    >> "$work/inputs.before.sha256"
gcc --version > "$work/compiler.txt"

run gcc $flags -c "$pkg/test/runtime.c" -o "$work/runtime.o"
run gcc $flags "$pkg/test/test_plist.c" "$work/runtime.o" $links -o "$work/core"
run timeout 90s "$work/core"

sources="$pkg/plist_native.c $pkg/xrt_plist_tree.c $pkg/xrt_plist_io.c
$pkg/xrt_plist_xml.c $pkg/xrt_plist_binary.c $pkg/xrt_plist_xml_writer.c
$pkg/xrt_plist_binary_writer.c
release/extlibs/xml.dom/xrt_xml_dom_tree.c
release/extlibs/xml.dom/xrt_xml_dom_parser.c
release/extlibs/xml.dom/xrt_xml_dom_writer.c
release/extlibs/xml.sax/xrt_xml_common.c
release/extlibs/xml.sax/xrt_xml_sax.c
release/extlibs/_expat/vendor/expat/lib/xmlparse.c
release/extlibs/_expat/vendor/expat/lib/xmlrole.c
release/extlibs/_expat/vendor/expat/lib/xmltok.c
release/extlibs/_expat/expat_random.c"
run gcc $flags $sources "$pkg/test/test_native.c" "$work/runtime.o" \
    $links -o "$work/native"
run timeout 90s "$work/native"
run gcc $flags release/examples/extlibs/plist_c.c "$work/runtime.o" \
    $links -o "$work/example"
run timeout 30s "$work/example" > "$work/example_c.txt"
run timeout 30s "$work/example" "$work/c.xml" "$work/c.bplist" > "$work/example_c_files.txt"
run timeout 90s ./release/xl run release/extlibs/plist/test/test_plist.c
run timeout 90s ./release/xl run test/cases/extlib_plist.xl
run timeout 90s ./release/xl run release/extlibs/plist/test/boundaries.xl
run timeout 90s ./release/xl run release/extlibs/plist/test/allocations.xl
run timeout 90s ./release/xl run release/extlibs/plist/test/owned_results.xl
run timeout 90s ./release/xl cgen "$pkg/test/owned_probe.xl" --emit "$work/owned_probe.c"
for symbol in xplist_error_message xplist_stringify_text; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [ "$count" -ne 1 ]; then
        printf 'plist probe must directly adopt %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
copies=$(grep -Ec '= *xlir_string_copy *\(' "$work/owned_probe.c" || true)
if [ "$copies" -gt 5 ]; then
    printf 'plist controlled fixture regained redundant return copies\n' >&2
    exit 1
fi
if ./release/xl check "$pkg/test/private_native_document_negative.xl" \
    > "$work/private_native_document.txt" 2>&1; then
    printf 'plist: native document must not be exported\n' >&2
    exit 1
fi
if ! grep -Fq 'no such exported type' "$work/private_native_document.txt"; then
    cat "$work/private_native_document.txt" >&2
    printf 'plist: expected private type diagnostic\n' >&2
    exit 1
fi
run timeout 90s ./release/xl run release/examples/extlibs/plist.xl > "$work/example_xl.txt"
run timeout 90s ./release/xl run release/examples/extlibs/plist.xl -- \
    "$work/xl.xml" "$work/xl.bplist" > "$work/example_xl_files.txt"
run diff -u "$work/example_c.txt" "$work/example_xl.txt"
run diff -u "$work/example_c_files.txt" "$work/example_xl_files.txt"
run python3 -I "$pkg/test/check_external.py" "$work/c.xml" "$work/xl.xml" \
    "$work/c.bplist" "$work/xl.bplist"
for source in \
    "$pkg/test/smoke.xl" \
    test/cases/extlib_plist.xl \
    "$pkg/test/boundaries.xl" \
    "$pkg/test/allocations.xl" \
    "$pkg/test/owned_results.xl"; do
    name=$(basename "$source" .xl)
    run timeout 90s ./release/xl build "$source" -o "$work/$name"
    run timeout 30s "$work/$name"
done
run timeout 90s ./release/xl build release/examples/extlibs/plist.xl \
    -o "$work/plist_xlang"
run timeout 30s "$work/plist_xlang" "$work/aot.xml" "$work/aot.bplist" \
    > "$work/example_xlang_aot.txt"
run diff -u "$work/example_c_files.txt" "$work/example_xlang_aot.txt"
run python3 -I "$pkg/test/check_external.py" "$work/c.xml" "$work/aot.xml" \
    "$work/c.bplist" "$work/aot.bplist"
run timeout 90s ./release/xl build release/examples/extlibs/plist_c.c \
    -o "$work/plist_c"
run timeout 30s "$work/plist_c" "$work/c_aot.xml" "$work/c_aot.bplist" \
    > "$work/example_c_aot.txt"
run diff -u "$work/example_c_files.txt" "$work/example_c_aot.txt"
run python3 -I "$pkg/test/check_external.py" "$work/c.xml" "$work/c_aot.xml" \
    "$work/c.bplist" "$work/c_aot.bplist"
run sha256sum -c "$work/inputs.before.sha256" > "$work/inputs.after.txt"
printf 'plist Linux C/XLang ASan/UBSan and bilingual AOT gate: PASS\n'
