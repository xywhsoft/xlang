#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-rss-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}

flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie
       -Ilib/xrt -Irelease/extlibs/rss/include
       -Irelease/extlibs/xml.dom -Irelease/extlibs/xml.sax
       -Irelease/extlibs/_expat/vendor/expat/lib
       -Irelease/extlibs/_expat/vendor/expat -pthread)
sources=(
    release/extlibs/rss/test/runtime.c
    release/extlibs/_expat/vendor/expat/lib/xmlparse.c
    release/extlibs/_expat/vendor/expat/lib/xmlrole.c
    release/extlibs/_expat/vendor/expat/lib/xmltok.c
    release/extlibs/_expat/expat_random.c
    release/extlibs/xml.sax/xrt_xml_common.c
    release/extlibs/xml.sax/xrt_xml_sax.c
    release/extlibs/xml.sax/xml_sax_native.c
    release/extlibs/xml.dom/xrt_xml_dom_tree.c
    release/extlibs/xml.dom/xrt_xml_dom_parser.c
    release/extlibs/xml.dom/xrt_xml_dom_writer.c
    release/extlibs/xml.dom/xml_dom_native.c
    release/extlibs/rss/rss_parse.c
    release/extlibs/rss/rss_build.c
)

objects=()
for i in "${!sources[@]}"; do
    object="$work/lib_$i.o"
    "$cc" "${flags[@]}" -c "${sources[$i]}" -o "$object"
    objects+=("$object")
done
"$cc" "${flags[@]}" release/extlibs/rss/test/test_rss.c \
    "${objects[@]}" -lm -ldl -o "$work/test_rss"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
timeout 90s "$work/test_rss"

"$cc" "${flags[@]}" -Werror release/examples/extlibs/rss_c.c \
    "${objects[@]}" -lm -ldl -o "$work/rss_example"
"$cc" "${flags[@]}" -Werror release/extlibs/rss/examples/atom_author_c.c \
    "${objects[@]}" -lm -ldl -o "$work/atom_example"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 timeout 30s "$work/rss_example" > "$work/rss_c.txt"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 timeout 30s "$work/atom_example" > "$work/atom_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for RSS XLang gate\n' >&2
    exit 1
fi
(
    cd release
    timeout 90s ./xl run ../test/cases/extlib_rss.xl
    timeout 90s ./xl run extlibs/rss/test/author_inheritance.xl
    timeout 90s ./xl run extlibs/rss/test/input_limit.xl
    timeout 90s ./xl run examples/extlibs/rss_pair.xl > "$work/rss_xl.txt"
    timeout 90s ./xl run extlibs/rss/examples/atom_author.xl > "$work/atom_xl.txt"
    timeout 90s ./xl run examples/extlibs/rss.xl > "$work/rss_full.txt"
)
diff -u "$work/rss_c.txt" "$work/rss_xl.txt"
printf 'type: rss\nitems: 1\n' > "$work/rss_expected.txt"
diff -u "$work/rss_expected.txt" "$work/rss_c.txt"
diff -u "$work/atom_c.txt" "$work/atom_xl.txt"
printf 'Editor\nSource\n' > "$work/atom_expected.txt"
diff -u "$work/atom_expected.txt" "$work/atom_c.txt"
printf 'rss Linux C ASan/UBSan and XLang parity gate: PASS\n'
