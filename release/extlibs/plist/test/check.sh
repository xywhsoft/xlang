#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-plist-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cd "$root"

pkg=release/extlibs/plist
flags='-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread -Ilib/xrt -Irelease/extlibs/plist/include'
links='-fsanitize=address,undefined -pthread -ldl -lm'
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

gcc $flags -c "$pkg/test/runtime.c" -o "$work/runtime.o"
gcc $flags "$pkg/test/test_plist.c" "$work/runtime.o" $links -o "$work/core"
"$work/core"

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
gcc $flags $sources "$pkg/test/test_native.c" "$work/runtime.o" \
    $links -o "$work/native"
"$work/native"
gcc $flags release/examples/extlibs/plist_c.c "$work/runtime.o" \
    $links -o "$work/example"
"$work/example" > "$work/example_c.txt"
"$work/example" "$work/c.xml" "$work/c.bplist" > "$work/example_c_files.txt"
./release/xl run release/extlibs/plist/test/test_plist.c
./release/xl run test/cases/extlib_plist.xl
./release/xl run release/extlibs/plist/test/boundaries.xl
./release/xl run release/extlibs/plist/test/allocations.xl
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
./release/xl run release/examples/extlibs/plist.xl > "$work/example_xl.txt"
./release/xl run release/examples/extlibs/plist.xl -- \
    "$work/xl.xml" "$work/xl.bplist" > "$work/example_xl_files.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
diff -u "$work/example_c_files.txt" "$work/example_xl_files.txt"
python3 "$pkg/test/check_external.py" "$work/c.xml" "$work/xl.xml" \
    "$work/c.bplist" "$work/xl.bplist"
for source in \
    "$pkg/test/smoke.xl" \
    "$pkg/test/boundaries.xl" \
    "$pkg/test/allocations.xl"; do
    name=$(basename "$source" .xl)
    timeout 90s ./release/xl build "$source" -o "$work/$name"
    timeout 30s "$work/$name"
done
timeout 90s ./release/xl build release/examples/extlibs/plist.xl \
    -o "$work/plist_xlang"
timeout 30s "$work/plist_xlang" "$work/aot.xml" "$work/aot.bplist" \
    > "$work/example_xlang_aot.txt"
diff -u "$work/example_c_files.txt" "$work/example_xlang_aot.txt"
python3 "$pkg/test/check_external.py" "$work/c.xml" "$work/aot.xml" \
    "$work/c.bplist" "$work/aot.bplist"
timeout 90s ./release/xl build release/examples/extlibs/plist_c.c \
    -o "$work/plist_c"
timeout 30s "$work/plist_c" "$work/c_aot.xml" "$work/c_aot.bplist" \
    > "$work/example_c_aot.txt"
diff -u "$work/example_c_files.txt" "$work/example_c_aot.txt"
python3 "$pkg/test/check_external.py" "$work/c.xml" "$work/c_aot.xml" \
    "$work/c.bplist" "$work/c_aot.bplist"
printf 'plist Linux C/XLang ASan/UBSan and bilingual AOT gate: PASS\n'
