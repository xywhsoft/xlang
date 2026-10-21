#define XRT_MODULE_VALUE
#include "xrt.h"
#include "../include/pkcs_native.h"
#include <stdio.h>

int main(void)
{
    char* oid = NULL;
    char* null_value = NULL;
    char* algorithm_content = NULL;
    char* algorithm = NULL;
    char* algorithm_tag = NULL;
    char* dotted = NULL;
    char* serial = NULL;
    char* octet = NULL;
    char* octet_content = NULL;
    char* inner = NULL;
    char* sequence = NULL;
    char* set_first = NULL;
    char* set_content = NULL;
    char* set_value = NULL;
    char* bit_string = NULL;
    int64_t decoded = 0;
    int64_t tag = -1;
    int64_t next = -1;
    int64_t total_bytes = -1;
    int64_t content_len = -1;
    int status = 1;

    oid = xpkcs_encode_oid("1.2.840.113549.1.1.1");
    null_value = xpkcs_encode_null();
    if (oid == NULL || null_value == NULL) goto done;
    algorithm_content = xpkcs_concat(oid, null_value);
    if (algorithm_content == NULL) goto done;
    algorithm = xpkcs_wrap_sequence(algorithm_content);
    if (algorithm == NULL) goto done;
    algorithm_tag = xpkcs_tag_name(algorithm);
    dotted = xpkcs_decode_oid(oid);
    serial = xpkcs_encode_int(12345);
    if (algorithm_tag == NULL || dotted == NULL || serial == NULL) goto done;
    if (!xpkcs_decode_int(serial, &decoded)) goto done;
    tag = xpkcs_get_tag(serial);
    if (tag < 0) goto done;

    printf("AlgorithmIdentifier DER:  %s\n", algorithm);
    printf("  tag:  %s\n", algorithm_tag);
    printf("  OID:  %s\n", dotted);
    printf("serial INTEGER:  %s\n", serial);
    printf("  value:  %lld\n", (long long)decoded);
    printf("  tag byte:  %lld\n", (long long)tag);

    octet = xpkcs_encode_octet("cafebabe");
    if (octet == NULL) goto done;
    octet_content = xpkcs_get_content(octet);
    if (octet_content == NULL) goto done;
    inner = xpkcs_concat(serial, oid);
    if (inner == NULL) goto done;
    sequence = xpkcs_wrap_sequence(inner);
    set_first = xpkcs_encode_int(1);
    if (set_first == NULL) goto done;
    set_content = xpkcs_concat(set_first, serial);
    if (set_content == NULL) goto done;
    set_value = xpkcs_wrap_set(set_content);
    bit_string = xpkcs_encode_bitstring("a0");
    if (sequence == NULL || set_value == NULL || bit_string == NULL) goto done;
    next = xpkcs_next_offset(inner, 0);
    total_bytes = xpkcs_der_len(sequence);
    content_len = xpkcs_get_length(sequence);
    if (next < 0 || total_bytes < 0 || content_len < 0) goto done;
    printf("OCTET STRING:  %s\n", octet);
    printf("  content:  %s\n", octet_content);
    printf("SEQUENCE:  %s\n", sequence);
    printf("  total bytes:  %lld\n", (long long)total_bytes);
    printf("  content len:  %lld\n", (long long)content_len);
    printf("  next item offset:  %lld\n", (long long)next);
    printf("SET:  %s\n", set_value);
    printf("BIT STRING:  %s\n", bit_string);
    puts("pkcs example: PASS");
    status = 0;

done:
    xrtFree(bit_string);
    xrtFree(set_value);
    xrtFree(set_content);
    xrtFree(set_first);
    xrtFree(sequence);
    xrtFree(inner);
    xrtFree(octet_content);
    xrtFree(octet);
    xrtFree(serial);
    xrtFree(dotted);
    xrtFree(algorithm_tag);
    xrtFree(algorithm);
    xrtFree(algorithm_content);
    xrtFree(null_value);
    xrtFree(oid);
    return status;
}

/* TCC 单文件范例编入实现；正式包仍按清单单独编译。 */
#ifdef __TINYC__
#include "../pkcs_native.c"
#endif
