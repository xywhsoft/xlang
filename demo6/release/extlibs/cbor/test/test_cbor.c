/* CBOR C API smoke: xvalue containers and byte buffers share one codec. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

xbuffer* xcbor_encode(xvalue* pValue);
xvalue* xcbor_decode(const void* pData, size_t iSize);
xbuffer* xcbor_encode_bytes(const void* pData, size_t iSize);
xbuffer* xcbor_decode_bytes(const void* pData, size_t iSize);

static int fail(const char* sMessage)
{
    fprintf(stderr, "cbor C smoke failed: %s\n", sMessage);
    return 1;
}

int main(void)
{
    xvalue* pSource = xrtValueObject();
    xvalue* pName = xrtValueString(XRT_STR_LITERAL("xlang"));
    xvalue* pVersion = xrtValueInt(6);
    xvalue* pEncodedValue;
    xbuffer* pEncoded;
    xstrview Name;
    const unsigned char aBytes[] = { 0xdeu, 0xadu, 0xbeu, 0xefu };
    xbuffer* pBinary;
    xbuffer* pDecodedBytes;
    int iResult = 1;

    if (pSource == NULL || pName == NULL || pVersion == NULL ||
        !xrtValueObjectSetTake(pSource, XRT_STR_LITERAL("name"), &pName) ||
        !xrtValueObjectSetTake(pSource, XRT_STR_LITERAL("version"), &pVersion)) {
        return fail("could not build source value");
    }
    pEncoded = xcbor_encode(pSource);
    xrtValueRelease(pSource);
    if (pEncoded == NULL) { return fail("encode failed"); }

    pEncodedValue = xcbor_decode(pEncoded->Data, pEncoded->Size);
    xrtBufferDestroy(pEncoded);
    if (pEncodedValue == NULL || xrtValueType(pEncodedValue) != XVALUE_OBJECT ||
        !xrtValueGetString(xrtValueObjectGet(pEncodedValue, XRT_STR_LITERAL("name")), &Name) ||
        Name.Size != 5u || memcmp(Name.Data, "xlang", 5u) != 0) {
        xrtValueRelease(pEncodedValue);
        return fail("object roundtrip failed");
    }
    xrtValueRelease(pEncodedValue);

    pBinary = xcbor_encode_bytes(aBytes, sizeof(aBytes));
    if (pBinary == NULL) { return fail("binary encode failed"); }
    pDecodedBytes = xcbor_decode_bytes(pBinary->Data, pBinary->Size);
    xrtBufferDestroy(pBinary);
    if (pDecodedBytes == NULL || pDecodedBytes->Size != sizeof(aBytes) ||
        memcmp(pDecodedBytes->Data, aBytes, sizeof(aBytes)) != 0) {
        xrtBufferDestroy(pDecodedBytes);
        return fail("binary roundtrip failed");
    }
    xrtBufferDestroy(pDecodedBytes);
    puts("cbor C smoke: PASS");
    iResult = 0;
    return iResult;
}

#include "../cbor_native.c"
