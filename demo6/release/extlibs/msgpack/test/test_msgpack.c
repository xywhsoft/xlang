/* MessagePack C smoke: xvalue maps and byte strings use the same codec. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

xbuffer* xmsgpack_encode(xvalue* pValue);
xvalue* xmsgpack_decode(const void* pData, size_t iSize);
xbuffer* xmsgpack_encode_bytes(const void* pData, size_t iSize);
xbuffer* xmsgpack_decode_bytes(const void* pData, size_t iSize);

int main(void)
{
    xvalue* pSource = xrtValueObject();
    xvalue* pName = xrtValueString(XRT_STR_LITERAL("xlang"));
    xbuffer* pPacked;
    xvalue* pDecoded;
    xstrview Text;
    const uint8 aBytes[] = { 0xcau, 0xfeu };
    xbuffer* pBinary;
    xbuffer* pRestored;

    if (pSource == NULL || pName == NULL ||
        !xrtValueObjectSetTake(pSource, XRT_STR_LITERAL("name"), &pName)) {
        return 1;
    }
    pPacked = xmsgpack_encode(pSource);
    xrtValueRelease(pSource);
    pDecoded = pPacked == NULL ? NULL : xmsgpack_decode(pPacked->Data, pPacked->Size);
    xrtBufferDestroy(pPacked);
    if (pDecoded == NULL || !xrtValueGetString(xrtValueObjectGet(pDecoded, XRT_STR_LITERAL("name")), &Text) ||
        Text.Size != 5u || memcmp(Text.Data, "xlang", 5u) != 0) {
        xrtValueRelease(pDecoded);
        return 1;
    }
    xrtValueRelease(pDecoded);

    pBinary = xmsgpack_encode_bytes(aBytes, sizeof(aBytes));
    pRestored = pBinary == NULL ? NULL : xmsgpack_decode_bytes(pBinary->Data, pBinary->Size);
    xrtBufferDestroy(pBinary);
    if (pRestored == NULL || pRestored->Size != sizeof(aBytes) || memcmp(pRestored->Data, aBytes, sizeof(aBytes)) != 0) {
        xrtBufferDestroy(pRestored);
        return 1;
    }
    xrtBufferDestroy(pRestored);
    puts("msgpack C smoke: PASS");
    return 0;
}

#include "../msgpack_native.c"
