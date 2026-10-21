#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-sitemap-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/sitemap/include
       -fsanitize=address,undefined -fno-omit-frame-pointer
       -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/sitemap/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
sources=(
    release/extlibs/_expat/vendor/expat/lib/xmlparse.c
    release/extlibs/_expat/vendor/expat/lib/xmlrole.c
    release/extlibs/_expat/vendor/expat/lib/xmltok.c
    release/extlibs/_expat/expat_random.c
    release/extlibs/xml.sax/xrt_xml_common.c
    release/extlibs/xml.sax/xrt_xml_sax.c
    release/extlibs/xml.sax/xml_sax_native.c
    release/extlibs/sitemap/sitemap_xml.c
    release/extlibs/sitemap/sitemap_robots.c
)
objects=("$work/xrt_runtime.o")
for i in "${!sources[@]}"; do
    object="$work/lib_$i.o"
    "$cc" "${flags[@]}" -c "${sources[$i]}" -o "$object"
    objects+=("$object")
done
"$cc" "${flags[@]}" release/extlibs/sitemap/test/test_sitemap.c \
    "${objects[@]}" -lm -ldl -o "$work/test_sitemap"
timeout 90s "$work/test_sitemap"
"$cc" "${flags[@]}" release/extlibs/sitemap/examples/sitemap_c.c \
    "${objects[@]}" -lm -ldl -o "$work/sitemap_example"
timeout 30s "$work/sitemap_example" > "$work/example_c.txt"

if [[ ! -x release/xl ]]; then
    printf 'missing Linux release/xl for sitemap XLang gate\n' >&2
    exit 1
fi
timeout 90s release/xl run test/cases/extlib_sitemap.xl
timeout 90s release/xl run release/extlibs/sitemap/test/boundaries.xl
timeout 90s release/xl run release/extlibs/sitemap/test/allocations.xl
timeout 90s release/xl run release/extlibs/sitemap/test/protocol_limits.xl
timeout 90s release/xl run release/examples/extlibs/sitemap_pair.xl > "$work/example_xl.txt"
diff -u "$work/example_c.txt" "$work/example_xl.txt"
if ! command -v python3 >/dev/null; then
    printf 'python3 is required for the sitemap XML gate\n' >&2
    exit 1
fi
python3 -I release/extlibs/sitemap/test/check_external.py "$work/example_c.txt" "$work/example_xl.txt"
printf 'sitemap Linux C ASan/UBSan, XLang and XML gate: PASS\n'
