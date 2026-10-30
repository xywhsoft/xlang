#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$root"
work=$(mktemp -d "${TMPDIR:-/tmp}/xlang-pkcs-XXXXXX")
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
    if ((result == 0)); then
        rm -rf -- "$work" || result=$?
    else
        printf '[pkcs] FAIL exit=%s artifacts=%s\n' "$result" "$work" >&2
    fi
    printf '[pkcs] report: %s.report.txt\n' "$work"
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
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -Ilib/xrt
       -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie -pthread)
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export XLANG_PACKAGE_PATH="$root/release/extlibs"
input_paths() {
    find release/extlibs/pkcs release/include -type f -print0
    printf '%s\0' lib/xrt/xrt.h lib/xruntime/xruntime.h release/xl release/libxl.so release/libxlc.so \
        test/cases/extlib_pkcs.xl release/examples/extlibs/pkcs.xl
    if [[ -n "$cc_path" ]]; then printf '%s\0' "$cc_path"; fi
}
mapfile -d '' inputs < <(input_paths | sort -zu)
sha256sum "${inputs[@]}" > "$work/inputs.before.sha256"
"$cc" --version > "$work/compiler.txt"
compile() { timeout 180s "$cc" "$@"; }

compile -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -x c -fsyntax-only release/extlibs/pkcs/include/pkcs_native.h
compile -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Ilib/xrt \
    -DXRT_MODULE_VALUE -x c -fsyntax-only \
    release/extlibs/pkcs/include/pkcs_native.h

compile "${flags[@]}" -c release/extlibs/pkcs/test/xrt_runtime.c \
    -o "$work/xrt_runtime.o"
compile "${flags[@]}" release/extlibs/pkcs/pkcs_native.c \
    release/extlibs/pkcs/test/test_pkcs.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/test_pkcs"
timeout 90s "$work/test_pkcs"
compile "${flags[@]}" release/extlibs/pkcs/pkcs_native.c \
    release/extlibs/pkcs/examples/pkcs_c.c "$work/xrt_runtime.o" \
    -lm -ldl -o "$work/pkcs_example"
timeout 30s "$work/pkcs_example" > "$work/c_example.txt"
timeout 30s ./release/xl run release/extlibs/pkcs/examples/pkcs_c.c \
    > "$work/direct_c_example.txt"
diff -u "$work/c_example.txt" "$work/direct_c_example.txt"
timeout 90s ./release/xl run test/cases/extlib_pkcs.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/boundaries.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/allocations.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/owned_results.xl
timeout 90s ./release/xl run release/extlibs/pkcs/test/oid_errors.xl
timeout 90s ./release/xl cgen release/examples/extlibs/pkcs.xl --emit "$work/module.c"
grep -E '^extern .*xpkcs_[a-z0-9_]+[(]' "$work/module.c" | sort -u > "$work/abi.decls"
grep -oE '"xpkcs_[a-z0-9_]+"' release/extlibs/pkcs/src/pkcs.xl | tr -d '"' | sort -u > "$work/abi.expected"
grep -oE 'xpkcs_[a-z0-9_]+[(]' "$work/abi.decls" | tr -d '(' | sort -u > "$work/abi.actual"
test "$(wc -l < "$work/abi.decls")" -eq 16
diff -u "$work/abi.expected" "$work/abi.actual"
printf '#include "pkcs_native.h"\n' > "$work/abi.c"
cat "$work/abi.decls" >> "$work/abi.c"
compile "${flags[@]}" -Irelease/extlibs/pkcs/include -fsyntax-only "$work/abi.c"
timeout 90s ./release/xl cgen release/extlibs/pkcs/test/owned_probe.xl --emit "$work/owned_probe.c"
for symbol in get_content encode_int encode_oid decode_oid wrap_sequence wrap_set \
              encode_octet encode_null encode_bitstring concat tag_name; do
    count=$(grep -Ec "[[:alnum:]_]+\.Data[[:space:]]*=[[:space:]]*xpkcs_${symbol}[[:space:]]*\(" \
        "$work/owned_probe.c")
    if [[ $count != 1 ]]; then
        printf 'pkcs: whole-return probe must cover %s exactly once\n' "$symbol" >&2
        exit 1
    fi
done
if grep -Eq '= *xlir_string_copy *\(' "$work/owned_probe.c"; then
    printf 'pkcs: adopted return text was copied again\n' >&2
    exit 1
fi
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
printf '%s' 0603883703 | xxd -r -p > "$work/large_first_arc.der"
openssl asn1parse -inform DER -in "$work/large_first_arc.der" -i > "$work/large_first_arc.txt"
grep -q 'cons: SEQUENCE' "$work/algorithm.txt"
grep -q 'OBJECT.*rsaEncryption' "$work/algorithm.txt"
grep -q 'prim:  NULL' "$work/algorithm.txt"
grep -q 'INTEGER.*:3039' "$work/serial.txt"
grep -q 'OCTET STRING.*CAFEBABE' "$work/octet.txt"
grep -q 'cons: SET' "$work/set.txt"
grep -q 'INTEGER.*:01' "$work/set.txt"
grep -q 'INTEGER.*:3039' "$work/set.txt"
grep -q 'BIT STRING' "$work/bit_string.txt"
grep -q 'OBJECT.*2.999.3' "$work/large_first_arc.txt"

timeout 90s ./release/xl build release/extlibs/pkcs/test/boundaries.xl -o "$work/pkcs_boundaries"
timeout 30s "$work/pkcs_boundaries"
for name in central allocations owned_results oid_errors; do
    source="release/extlibs/pkcs/test/$name.xl"
    if [[ $name == central ]]; then source=test/cases/extlib_pkcs.xl; fi
    timeout 90s ./release/xl build "$source" -o "$work/pkcs_$name"
    timeout 30s "$work/pkcs_$name"
done
timeout 90s ./release/xl build release/examples/extlibs/pkcs.xl -o "$work/pkcs_xl_aot"
timeout 30s "$work/pkcs_xl_aot" > "$work/xl_aot.txt"
diff -u "$work/c_example.txt" "$work/xl_aot.txt"
timeout 90s ./release/xl build release/extlibs/pkcs/examples/pkcs_c.c -o "$work/pkcs_c_aot"
timeout 30s "$work/pkcs_c_aot" > "$work/c_aot.txt"
diff -u "$work/c_example.txt" "$work/c_aot.txt"
# 不把语言非空字符串的兜底异常当作 C/XRT 错误合同已完善。
compile "${flags[@]}" -DNDEBUG release/extlibs/pkcs/test/oid_errors_c.c \
    release/extlibs/pkcs/pkcs_native.c "$work/xrt_runtime.o" -lm -ldl -o "$work/oid_errors"
timeout 90s "$work/oid_errors"
printf 'pkcs Linux C ASan/UBSan, OpenSSL interop and bilingual AOT gate: PASS\n'
