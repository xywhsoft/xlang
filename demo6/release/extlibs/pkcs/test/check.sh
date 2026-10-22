#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-pkcs-XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cc=${CC:-gcc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -x c -fsyntax-only release/extlibs/pkcs/include/pkcs_native.h
"$cc" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -DXRT_MODULE_VALUE -x c -fsyntax-only \
    release/extlibs/pkcs/include/pkcs_native.h

"$cc" "${flags[@]}" -c release/extlibs/pkcs/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
"$cc" "${flags[@]}" release/extlibs/pkcs/pkcs_native.c \
    release/extlibs/pkcs/test/test_pkcs.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_pkcs"
timeout 90s "$work/test_pkcs"
"$cc" "${flags[@]}" release/extlibs/pkcs/pkcs_native.c \
    release/extlibs/pkcs/examples/pkcs_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/pkcs_example"
timeout 30s "$work/pkcs_example" > "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/pkcs/examples/pkcs_c.c \
    > "$work/direct_c_example.txt"
diff -u "$work/c_example.txt" "$work/direct_c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_pkcs.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/allocations.xl
timeout 90s ./release/xl run release/examples/extlibs/pkcs.xl \
    > "$work/xl_example.txt"
diff -u "$work/c_example.txt" "$work/xl_example.txt"

# 用独立的 ASN.1 实现读取范例生成的 DER，而不只做包内编解码互证。
command -v openssl >/dev/null
command -v xxd >/dev/null
algorithm=$(sed -n 's/^AlgorithmIdentifier DER:  //p' "$work/c_example.txt")
serial=$(sed -n 's/^serial INTEGER:  //p' "$work/c_example.txt")
octet=$(sed -n 's/^OCTET STRING:  //p' "$work/c_example.txt")
set_value=$(sed -n 's/^SET:  //p' "$work/c_example.txt")
bit_string=$(sed -n 's/^BIT STRING:  //p' "$work/c_example.txt")
test -n "$algorithm" && test -n "$serial" && test -n "$octet"
test -n "$set_value" && test -n "$bit_string"
printf '%s' "$algorithm" | xxd -r -p > "$work/algorithm.der"
printf '%s' "$serial" | xxd -r -p > "$work/serial.der"
printf '%s' "$octet" | xxd -r -p > "$work/octet.der"
printf '%s' "$set_value" | xxd -r -p > "$work/set.der"
printf '%s' "$bit_string" | xxd -r -p > "$work/bit_string.der"
openssl asn1parse -inform DER -in "$work/algorithm.der" -i > "$work/algorithm.txt"
openssl asn1parse -inform DER -in "$work/serial.der" -i > "$work/serial.txt"
openssl asn1parse -inform DER -in "$work/octet.der" -i > "$work/octet.txt"
openssl asn1parse -inform DER -in "$work/set.der" -i > "$work/set.txt"
openssl asn1parse -inform DER -in "$work/bit_string.der" -i > "$work/bit_string.txt"
grep -q 'cons: SEQUENCE' "$work/algorithm.txt"
grep -q 'OBJECT.*rsaEncryption' "$work/algorithm.txt"
grep -q 'prim:  NULL' "$work/algorithm.txt"
grep -q 'INTEGER.*:3039' "$work/serial.txt"
grep -q 'OCTET STRING.*CAFEBABE' "$work/octet.txt"
grep -q 'cons: SET' "$work/set.txt"
grep -q 'INTEGER.*:01' "$work/set.txt"
grep -q 'INTEGER.*:3039' "$work/set.txt"
grep -q 'BIT STRING' "$work/bit_string.txt"
printf 'pkcs Linux C ASan/UBSan and XLang parity gate: PASS\n'
