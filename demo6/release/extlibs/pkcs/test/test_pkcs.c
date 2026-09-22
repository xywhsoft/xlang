/* PKCS C smoke: the same DER helpers used by the xlang package. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

long long xpkcs_get_tag(cstr sHex);
long long xpkcs_get_length(cstr sHex);
cstr xpkcs_get_content(cstr sHex);
cstr xpkcs_encode_int(long long iValue);
long long xpkcs_decode_int(cstr sHex);
cstr xpkcs_encode_oid(cstr sDotted);
cstr xpkcs_decode_oid(cstr sHex);
cstr xpkcs_wrap_sequence(cstr sHex);
cstr xpkcs_encode_null(void);
cstr xpkcs_concat(cstr sLeft, cstr sRight);



static int fail(cstr sMessage)
{
    fprintf(stderr, "pkcs C smoke failed: %s\n", sMessage);
    return 1;
}



int main(void)
{
    cstr sInteger = xpkcs_encode_int(-128);
    cstr sOid = xpkcs_encode_oid("1.2.840.113549.1.1.1");
    cstr sNull = xpkcs_encode_null();
    cstr sSequence;

    if (strcmp(sInteger, "020180") != 0 || xpkcs_decode_int(sInteger) != -128) {
        return fail("signed integer round trip");
    }
    if (strcmp(sOid, "06092a864886f70d010101") != 0 ||
        strcmp(xpkcs_decode_oid(sOid), "1.2.840.113549.1.1.1") != 0) {
        return fail("OID round trip");
    }
    sSequence = xpkcs_wrap_sequence(xpkcs_concat(sOid, sNull));
    if (xpkcs_get_tag(sSequence) != 0x30 || xpkcs_get_length(sSequence) != 13 ||
        strcmp(xpkcs_get_content(sSequence), "06092a864886f70d0101010500") != 0) {
        return fail("AlgorithmIdentifier sequence");
    }
    puts("pkcs C smoke: PASS");
    return 0;
}
