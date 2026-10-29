#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-dns-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror
       -Ilib/xrt -Irelease/extlibs/dns -fsanitize=address,undefined
       -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" "${flags[@]}" -c release/extlibs/dns/test/xrt_runtime.c -o "$work/xrt.o"
"$cc" "${flags[@]}" release/extlibs/dns/test/test_dns.c \
    release/extlibs/dns/dns_native.c "$work/xrt.o" -lm -ldl -o "$work/test_dns"
timeout 30s "$work/test_dns"
"$cc" "${flags[@]}" release/extlibs/dns/examples/basic.c \
    release/extlibs/dns/dns_native.c "$work/xrt.o" -lm -ldl -o "$work/example"
timeout 30s "$work/example" > "$work/c.txt"
sed 's/\r$//' release/extlibs/dns/examples/basic.expected.txt > "$work/expected.txt"
diff -u "$work/expected.txt" "$work/c.txt"

xl=release/xl
if [[ ! -x "$xl" ]]; then
    printf 'dns Linux gate: release/xl is required\n' >&2
    exit 1
fi
timeout 90s "$xl" run test/cases/extlib_dns.xl
timeout 90s "$xl" run release/extlibs/dns/test/boundaries.xl
timeout 90s "$xl" run release/extlibs/dns/test/owned_text.xl
timeout 90s "$xl" run release/examples/extlibs/dns.xl > "$work/xl.txt"
diff -u "$work/expected.txt" "$work/xl.txt"
timeout 90s "$xl" cgen release/extlibs/dns/test/owned_text.xl --emit "$work/dns.c"
grep -Eq 'extern char\* xdns_encode_name\(' "$work/dns.c"
grep -Eq 'v[0-9]+\.Data[[:space:]]*=[[:space:]]*xdns_encode_name[[:space:]]*\(' "$work/dns.c"
if grep -q 'xdns_encode_name_xl' "$work/dns.c"; then
    printf 'dns Linux gate: obsolete point bridge remains\n' >&2
    exit 1
fi
timeout 90s "$xl" build release/extlibs/dns/test/boundaries.xl -o "$work/boundaries"
timeout 30s "$work/boundaries"
timeout 90s "$xl" build release/extlibs/dns/test/owned_text.xl -o "$work/owned_text"
timeout 30s "$work/owned_text"
timeout 90s "$xl" build release/examples/extlibs/dns.xl -o "$work/xl_example"
timeout 30s "$work/xl_example" > "$work/xl_aot.txt"
diff -u "$work/expected.txt" "$work/xl_aot.txt"
timeout 90s "$xl" build release/extlibs/dns/examples/basic.c \
    release/extlibs/dns/dns_native.c -o "$work/c_example"
timeout 30s "$work/c_example" > "$work/c_aot.txt"
diff -u "$work/expected.txt" "$work/c_aot.txt"
printf 'dns Linux C ASan/UBSan and XLang gate: PASS\n'
