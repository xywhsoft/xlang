/* CBOR RFC 8949 的 xvalue/xbuffer 绑定。 */
#include <limits.h>
#include <math.h>
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_BUFFER_HEX
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "cbor_native.h"

#define XCBOR_DOMAIN "xlang.extlib.cbor"
#define XCBOR_MAX_DEPTH 64u
#define XCBOR_MAX_ITEMS 1000000u
#define XCBOR_MAX_BYTES (64u * 1024u * 1024u)

typedef struct xcborreader {
    const uint8* Data;
    size_t Size;
    size_t Offset;
    size_t Items;
} xcborreader;

static void xcbor_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XCBOR_DOMAIN, 0, sMessage);
}

static bool xcbor_append(xbuffer* pOutput, const void* pData, size_t iSize)
{
    if (iSize > XCBOR_MAX_BYTES || pOutput->Size > XCBOR_MAX_BYTES - iSize) {
        xcbor_error(XERR_RANGE, "CBOR output exceeds the 64 MiB safety limit");
        return false;
    }
    if (!xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize })) {
        xcbor_error(XERR_MEMORY, "could not grow CBOR output");
        return false;
    }
    return true;
}

static bool xcbor_byte(xbuffer* pOutput, uint8 iByte)
{
    return xcbor_append(pOutput, &iByte, 1u);
}

static bool xcbor_head(xbuffer* pOutput, uint8 iMajor, uint64 iValue)
{
    uint8 aBytes[9];
    size_t i;

    if (iValue < 24u) {
        return xcbor_byte(pOutput, (uint8)((iMajor << 5u) | (uint8)iValue));
    }
    if (iValue <= UINT8_MAX) {
        aBytes[0] = (uint8)((iMajor << 5u) | 24u);
        aBytes[1] = (uint8)iValue;
        return xcbor_append(pOutput, aBytes, 2u);
    }
    if (iValue <= UINT16_MAX) {
        aBytes[0] = (uint8)((iMajor << 5u) | 25u);
        for (i = 0u; i < 2u; ++i) {
            aBytes[1u + i] = (uint8)(iValue >> ((1u - i) * 8u));
        }
        return xcbor_append(pOutput, aBytes, 3u);
    }
    if (iValue <= UINT32_MAX) {
        aBytes[0] = (uint8)((iMajor << 5u) | 26u);
        for (i = 0u; i < 4u; ++i) {
            aBytes[1u + i] = (uint8)(iValue >> ((3u - i) * 8u));
        }
        return xcbor_append(pOutput, aBytes, 5u);
    }
    aBytes[0] = (uint8)((iMajor << 5u) | 27u);
    for (i = 0u; i < 8u; ++i) {
        aBytes[1u + i] = (uint8)(iValue >> ((7u - i) * 8u));
    }
    return xcbor_append(pOutput, aBytes, 9u);
}

static bool xcbor_text(xbuffer* pOutput, xstrview Text)
{
    if (!xrtUtf8Valid(Text, NULL)) {
        xcbor_error(XERR_VALUE, "CBOR text is not valid UTF-8");
        return false;
    }
    return xcbor_head(pOutput, 3u, (uint64)Text.Size) &&
        xcbor_append(pOutput, Text.Data, Text.Size);
}

static bool xcbor_bytes(xbuffer* pOutput, xbytesview Data)
{
    return xcbor_head(pOutput, 2u, (uint64)Data.Size) &&
        xcbor_append(pOutput, Data.Data, Data.Size);
}

static bool xcbor_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems);

static bool xcbor_array(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    size_t i;
    size_t iCount = xrtValueCount(pValue);

    if (iCount > XCBOR_MAX_ITEMS - *pItems) {
        xcbor_error(XERR_RANGE, "CBOR document has too many array items");
        return false;
    }
    *pItems += iCount;
    if (!xcbor_head(pOutput, 4u, (uint64)iCount)) { return false; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pItem = xrtValueArrayGet(pValue, i);
        if (pItem == NULL || !xcbor_value(pOutput, pItem, iDepth + 1u, pItems)) {
            if (pItem == NULL) {
                xcbor_error(XERR_VALUE, "CBOR array contains an invalid value");
            }
            return false;
        }
    }
    return true;
}

static bool xcbor_object(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    size_t i;
    size_t iCount = xrtValueCount(pValue);

    if (iCount > XCBOR_MAX_ITEMS - *pItems) {
        xcbor_error(XERR_RANGE, "CBOR document has too many map items");
        return false;
    }
    *pItems += iCount;
    if (!xcbor_head(pOutput, 5u, (uint64)iCount)) { return false; }
    for (i = 0u; i < iCount; ++i) {
        xstrview Key;
        xvalue* pItem = xrtValueObjectAt(pValue, i, &Key);
        if (pItem == NULL || !xcbor_text(pOutput, Key) ||
            !xcbor_value(pOutput, pItem, iDepth + 1u, pItems)) {
            if (pItem == NULL) {
                xcbor_error(XERR_VALUE, "CBOR object contains an invalid value");
            }
            return false;
        }
    }
    return true;
}

static bool xcbor_time(xbuffer* pOutput, xtime iTime)
{
    str sText;
    bool bResult;

    sText = xrtTimeRFC3339(iTime, 0);
    if (sText == NULL) { return false; }
    bResult = xcbor_head(pOutput, 6u, 0u) && xcbor_text(pOutput, xrtStrView(sText));
    xrtFree(sText);
    return bResult;
}

static bool xcbor_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    xvaluetype Type;
    bool bValue;
    int64 iValue;
    uint64 iUnsigned;
    double fValue;
    xstrview Text;
    xbytesview Bytes;
    xtime Time;
    uint64 iBits;
    uint8 aBytes[9];
    size_t i;

    if (pValue == NULL) {
        xcbor_error(XERR_ARGUMENT, "CBOR value is missing");
        return false;
    }
    if (iDepth > XCBOR_MAX_DEPTH) {
        xcbor_error(XERR_RANGE, "CBOR nesting exceeds the depth limit");
        return false;
    }
    Type = xrtValueType(pValue);
    switch (Type) {
        case XVALUE_NULL:
            return xcbor_byte(pOutput, 0xf6u);
        case XVALUE_BOOL:
            return xrtValueGetBool(pValue, &bValue) &&
                xcbor_byte(pOutput, bValue ? 0xf5u : 0xf4u);
        case XVALUE_INT:
            if (!xrtValueGetInt(pValue, &iValue)) { return false; }
            return iValue >= 0 ?
                xcbor_head(pOutput, 0u, (uint64)iValue) :
                xcbor_head(pOutput, 1u, (uint64)(-(iValue + 1)));
        case XVALUE_UINT:
            return xrtValueGetUInt(pValue, &iUnsigned) &&
                xcbor_head(pOutput, 0u, iUnsigned);
        case XVALUE_FLOAT:
            if (!xrtValueGetFloat(pValue, &fValue)) { return false; }
            aBytes[0] = 0xfbu;
            memcpy(&iBits, &fValue, sizeof(iBits));
            for (i = 0u; i < 8u; ++i) {
                aBytes[1u + i] = (uint8)(iBits >> ((7u - i) * 8u));
            }
            return xcbor_append(pOutput, aBytes, sizeof(aBytes));
        case XVALUE_STRING:
            return xrtValueGetString(pValue, &Text) && xcbor_text(pOutput, Text);
        case XVALUE_BYTES:
            return xrtValueGetBytes(pValue, &Bytes) && xcbor_bytes(pOutput, Bytes);
        case XVALUE_TIME:
            if (iDepth == XCBOR_MAX_DEPTH) {
                xcbor_error(XERR_RANGE, "CBOR time tag exceeds the depth limit");
                return false;
            }
            return xrtValueGetTime(pValue, &Time) && xcbor_time(pOutput, Time);
        case XVALUE_ARRAY:
            return xcbor_array(pOutput, pValue, iDepth, pItems);
        case XVALUE_OBJECT:
            return xcbor_object(pOutput, pValue, iDepth, pItems);
        case XVALUE_INT_MAP:
        case XVALUE_SET:
            xcbor_error(XERR_UNSUPPORTED,
                "CBOR encode accepts array and dict; list and set need an explicit conversion");
            return false;
        default:
            xcbor_error(XERR_UNSUPPORTED, "CBOR cannot encode this dynamic value type");
            return false;
    }
}

static bool xcbor_need(xcborreader* pReader, size_t iSize)
{
    if (iSize > pReader->Size - pReader->Offset) {
        xcbor_error(XERR_VALUE, "truncated CBOR input");
        return false;
    }
    return true;
}

static bool xcbor_read_uint(xcborreader* pReader, uint8 iBytes, uint64* pValue)
{
    uint64 iValue = 0u;
    size_t i;

    if (!xcbor_need(pReader, (size_t)iBytes)) { return false; }
    for (i = 0u; i < (size_t)iBytes; ++i) {
        iValue = (iValue << 8u) | pReader->Data[pReader->Offset++];
    }
    *pValue = iValue;
    return true;
}

static bool xcbor_head_read(xcborreader* pReader, uint8* pMajor, uint8* pInfo,
    uint64* pValue)
{
    uint8 iHead;
    uint8 iInfo;

    if (!xcbor_need(pReader, 1u)) { return false; }
    iHead = pReader->Data[pReader->Offset++];
    *pMajor = iHead >> 5u;
    iInfo = iHead & 31u;
    *pInfo = iInfo;
    if (iInfo < 24u) { *pValue = iInfo; return true; }
    if (iInfo == 24u) { return xcbor_read_uint(pReader, 1u, pValue); }
    if (iInfo == 25u) { return xcbor_read_uint(pReader, 2u, pValue); }
    if (iInfo == 26u) { return xcbor_read_uint(pReader, 4u, pValue); }
    if (iInfo == 27u) { return xcbor_read_uint(pReader, 8u, pValue); }
    if (iInfo == 31u) { *pValue = UINT64_MAX; return true; }
    xcbor_error(XERR_VALUE, "reserved CBOR additional information");
    return false;
}

static double xcbor_half(uint16 bits)
{
    uint16 exponent = (bits >> 10u) & 31u;
    uint16 fraction = bits & 1023u;
    double result;
    if (exponent == 0u) result = ldexp((double)fraction, -24);
    else if (exponent == 31u) result = fraction ? NAN : INFINITY;
    else result = ldexp((double)(1024u + fraction), (int)exponent - 25);
    return (bits & 0x8000u) ? -result : result;
}

static xvalue* xcbor_read_value(xcborreader* pReader, size_t iDepth);

static xvalue* xcbor_read_string(xcborreader* pReader, uint8 iMajor, uint64 iLength)
{
    xvalue* pResult;
    if (iLength != UINT64_MAX) {
        if (iLength > SIZE_MAX || !xcbor_need(pReader, (size_t)iLength)) return NULL;
        if (iMajor == 3u && !xrtUtf8Valid((xstrview){
            (cstr)(pReader->Data + pReader->Offset), (size_t)iLength
        }, NULL)) {
            xcbor_error(XERR_VALUE, "CBOR text is not valid UTF-8");
            return NULL;
        }
        pResult = iMajor == 2u ?
            xrtValueBytes((xbytesview){ pReader->Data + pReader->Offset, (size_t)iLength }) :
            xrtValueString((xstrview){
                (cstr)(pReader->Data + pReader->Offset), (size_t)iLength
            });
        pReader->Offset += (size_t)iLength;
        return pResult;
    }
    {
        xbuffer* pJoined = xrtBufferCreate();
        if (!pJoined) return NULL;
        for (;;) {
            uint8 iChunkMajor, iInfo;
            uint64 iSize;
            if (!xcbor_need(pReader, 1u)) break;
            if (pReader->Data[pReader->Offset] == 0xffu) {
                pReader->Offset++;
                pResult = iMajor == 2u ?
                    xrtValueBytes((xbytesview){pJoined->Data, pJoined->Size}) :
                    xrtValueString((xstrview){(cstr)pJoined->Data, pJoined->Size});
                xrtBufferDestroy(pJoined);
                return pResult;
            }
            if (!xcbor_head_read(pReader, &iChunkMajor, &iInfo, &iSize) ||
                iChunkMajor != iMajor || iSize == UINT64_MAX ||
                iSize > XCBOR_MAX_BYTES - pJoined->Size ||
                !xcbor_need(pReader, (size_t)iSize) ||
                pReader->Items == XCBOR_MAX_ITEMS) {
                if (!xrtGetError()) xcbor_error(XERR_VALUE, "invalid CBOR string chunk");
                break;
            }
            if (iMajor == 3u && !xrtUtf8Valid((xstrview){
                (cstr)(pReader->Data + pReader->Offset), (size_t)iSize
            }, NULL)) {
                xcbor_error(XERR_VALUE, "CBOR text chunk is not valid UTF-8");
                break;
            }
            if (!xrtBufferAppend(pJoined, (xbytesview){
                pReader->Data + pReader->Offset, (size_t)iSize
            })) break;
            pReader->Offset += (size_t)iSize;
            pReader->Items++;
        }
        xrtBufferDestroy(pJoined);
        return NULL;
    }
}

static xvalue* xcbor_read_array(xcborreader* pReader, uint64 iCount, size_t iDepth)
{
    xvalue* pResult;
    uint64 i;

    if (iCount != UINT64_MAX && (iCount > XCBOR_MAX_ITEMS ||
        pReader->Items > XCBOR_MAX_ITEMS - (size_t)iCount)) {
        xcbor_error(XERR_RANGE, "CBOR array has too many items");
        return NULL;
    }
    if (iCount != UINT64_MAX) pReader->Items += (size_t)iCount;
    pResult = xrtValueArray();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; iCount == UINT64_MAX || i < iCount; ++i) {
        if (iCount == UINT64_MAX) {
            if (!xcbor_need(pReader, 1u)) break;
            if (pReader->Data[pReader->Offset] == 0xffu) {
                pReader->Offset++;
                return pResult;
            }
            if (pReader->Items == XCBOR_MAX_ITEMS) {
                xcbor_error(XERR_RANGE, "CBOR array has too many items");
                break;
            }
            pReader->Items++;
        }
        xvalue* pItem = xcbor_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueArrayAppendTake(pResult, &pItem)) {
            xrtValueRelease(pItem);
            break;
        }
    }
    if (iCount != UINT64_MAX && i == iCount) return pResult;
    xrtValueRelease(pResult);
    return NULL;
}

static xvalue* xcbor_read_object(xcborreader* pReader, uint64 iCount, size_t iDepth)
{
    xvalue* pResult;
    uint64 i;

    if (iCount != UINT64_MAX && (iCount > XCBOR_MAX_ITEMS ||
        pReader->Items > XCBOR_MAX_ITEMS - (size_t)iCount)) {
        xcbor_error(XERR_RANGE, "CBOR map has too many items");
        return NULL;
    }
    if (iCount != UINT64_MAX) pReader->Items += (size_t)iCount;
    pResult = xrtValueObject();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; iCount == UINT64_MAX || i < iCount; ++i) {
        xstrview Key;
        xvalue* pKey;
        xvalue* pItem;
        if (iCount == UINT64_MAX) {
            if (!xcbor_need(pReader, 1u)) break;
            if (pReader->Data[pReader->Offset] == 0xffu) {
                pReader->Offset++;
                return pResult;
            }
            if (pReader->Items == XCBOR_MAX_ITEMS) {
                xcbor_error(XERR_RANGE, "CBOR map has too many items");
                break;
            }
            pReader->Items++;
        }
        pKey = xcbor_read_value(pReader, iDepth + 1u);
        if (!pKey) break;
        if (xrtValueType(pKey) != XVALUE_STRING ||
            !xrtValueGetString(pKey, &Key)) {
            xcbor_error(XERR_VALUE, "CBOR map keys must be UTF-8 text");
            xrtValueRelease(pKey);
            break;
        }
        if (xrtValueObjectHas(pResult, Key)) {
            xcbor_error(XERR_VALUE, "duplicate CBOR map key");
            xrtValueRelease(pKey);
            break;
        }
        pItem = xcbor_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueObjectSetTake(pResult, Key, &pItem)) {
            xrtValueRelease(pItem);
            xrtValueRelease(pKey);
            break;
        }
        xrtValueRelease(pKey);
    }
    if (iCount != UINT64_MAX && i == iCount) return pResult;
    xrtValueRelease(pResult);
    return NULL;
}

static xvalue* xcbor_read_tag(xcborreader* pReader, uint64 iTag, size_t iDepth)
{
    xvalue* pValue;
    xtime Time;

    if (iTag != 0u && iTag != 1u) {
        xcbor_error(XERR_UNSUPPORTED, "unsupported CBOR semantic tag");
        return NULL;
    }
    pValue = xcbor_read_value(pReader, iDepth + 1u);
    if (pValue == NULL) { return NULL; }
    if (iTag == 0u) {
        xstrview Text;
        if (!xrtValueGetString(pValue, &Text) || !xrtTimeParseRFC3339(Text, &Time)) {
            xrtValueRelease(pValue);
            xcbor_error(XERR_VALUE, "CBOR tag 0 must contain RFC 3339 text");
            return NULL;
        }
        xrtValueRelease(pValue);
        return xrtValueTime(Time);
    }
    {
        int64 iSeconds;
        double fSeconds;
        if (xrtValueType(pValue) == XVALUE_INT &&
            xrtValueGetInt(pValue, &iSeconds)) {
            xrtValueRelease(pValue);
            if (!xrtTimeFromUnix(iSeconds, &Time)) { return NULL; }
            return xrtValueTime(Time);
        }
        if (xrtValueType(pValue) == XVALUE_FLOAT &&
            xrtValueGetFloat(pValue, &fSeconds) && isfinite(fSeconds)) {
            long double rounded = roundl((long double)fSeconds * 1000000.0L);
            if (rounded < -0x1p63L || rounded >= 0x1p63L) {
                xrtValueRelease(pValue);
                xcbor_error(XERR_RANGE, "CBOR epoch time is out of range");
                return NULL;
            }
            xrtValueRelease(pValue);
            return xrtValueTime((xtime)rounded);
        }
        xrtValueRelease(pValue);
        xcbor_error(XERR_VALUE, "CBOR tag 1 must contain epoch seconds");
        return NULL;
    }
}

static xvalue* xcbor_read_value(xcborreader* pReader, size_t iDepth)
{
    uint8 iMajor;
    uint8 iInfo;
    uint64 iValue;

    if (iDepth > XCBOR_MAX_DEPTH) {
        xcbor_error(XERR_RANGE, "CBOR nesting exceeds the depth limit");
        return NULL;
    }
    if (!xcbor_head_read(pReader, &iMajor, &iInfo, &iValue)) { return NULL; }
    if (iMajor == 7u) {
        if (iInfo == 31u) {
            xcbor_error(XERR_VALUE, "unexpected CBOR break marker");
            return NULL;
        }
        if (iInfo == 20u) { return xrtValueRetain(xrtValueBool(false)); }
        if (iInfo == 21u) { return xrtValueRetain(xrtValueBool(true)); }
        if (iInfo == 22u) { return xrtValueRetain(xrtValueNull()); }
        if (iInfo == 25u) return xrtValueFloat(xcbor_half((uint16)iValue));
        if (iInfo == 26u) {
            uint32 iBits = (uint32)iValue;
            float fValue;
            memcpy(&fValue, &iBits, sizeof(fValue));
            return xrtValueFloat((double)fValue);
        }
        if (iInfo == 27u) {
            uint64 iBits = iValue;
            double fValue;
            memcpy(&fValue, &iBits, sizeof(fValue));
            return xrtValueFloat(fValue);
        }
        xcbor_error(XERR_UNSUPPORTED, "unsupported CBOR simple or floating-point value");
        return NULL;
    }
    /* UINT64_MAX 也能由定长头部编码，只有附加信息 31 表示不定长。 */
    if (iInfo != 31u && iValue == UINT64_MAX && iMajor >= 2u && iMajor <= 5u) {
        xcbor_error(XERR_RANGE, "CBOR definite length exceeds the safety limit");
        return NULL;
    }
    switch (iMajor) {
        case 0u:
            return iValue <= INT64_MAX ? xrtValueInt((int64)iValue) :
                xrtValueUInt(iValue);
        case 1u:
            if (iValue > INT64_MAX) { xcbor_error(XERR_RANGE, "CBOR negative integer exceeds int64"); return NULL; }
            return xrtValueInt(-1 - (int64)iValue);
        case 2u:
        case 3u:
            return xcbor_read_string(pReader, iMajor, iValue);
        case 4u:
            return xcbor_read_array(pReader, iValue, iDepth);
        case 5u:
            return xcbor_read_object(pReader, iValue, iDepth);
        case 6u:
            return xcbor_read_tag(pReader, iValue, iDepth);
        default:
            xcbor_error(XERR_VALUE, "invalid CBOR major type");
            return NULL;
    }
}

static xbuffer* xcbor_encode_value(const xvalue* pValue)
{
    size_t iItems = 0u;
    xbuffer* pOutput = xrtBufferCreate();
    if (pOutput == NULL) { return NULL; }
    if (!xcbor_value(pOutput, pValue, 0u, &iItems)) {
        xrtBufferDestroy(pOutput);
        return NULL;
    }
    xrtClearError();
    return pOutput;
}

xbuffer* xcbor_encode(xvalue* pValue)
{
    xrtClearError();
    return xcbor_encode_value(pValue);
}

xvalue* xcbor_decode(const void* pData, size_t iSize)
{
    xcborreader Reader;
    xvalue* pValue;

    xrtClearError();
    if ((pData == NULL && iSize != 0u) || iSize == 0u) {
        xcbor_error(XERR_ARGUMENT, "CBOR input is missing");
        return NULL;
    }
    if (iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_RANGE, "CBOR input exceeds the 64 MiB safety limit");
        return NULL;
    }
    Reader = (xcborreader){ (const uint8*)pData, iSize, 0u, 0u };
    pValue = xcbor_read_value(&Reader, 0u);
    if (pValue == NULL || Reader.Offset != Reader.Size) {
        xrtValueRelease(pValue);
        if (pValue != NULL && Reader.Offset != Reader.Size) {
            xcbor_error(XERR_VALUE, "trailing bytes after the CBOR item");
        }
        return NULL;
    }
    xrtClearError();
    return pValue;
}

xbuffer* xcbor_encode_bytes(const void* pData, size_t iSize)
{
    xbuffer* pOutput;
    xrtClearError();
    if (pData == NULL && iSize != 0u) {
        xcbor_error(XERR_ARGUMENT, "CBOR byte input is missing");
        return NULL;
    }
    if (iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_RANGE, "CBOR byte input exceeds the 64 MiB safety limit");
        return NULL;
    }
    pOutput = xrtBufferCreate();
    if (pOutput == NULL) { return NULL; }
    if (!xcbor_bytes(pOutput, (xbytesview){ (cbytes)pData, iSize })) {
        xrtBufferDestroy(pOutput);
        return NULL;
    }
    xrtClearError();
    return pOutput;
}

xbuffer* xcbor_decode_bytes(const void* pData, size_t iSize)
{
    xvalue* pValue;
    xbytesview Data;
    xbuffer* pOutput;
    pValue = xcbor_decode(pData, iSize);
    if (!pValue || xrtValueType(pValue) != XVALUE_BYTES ||
        !xrtValueGetBytes(pValue, &Data)) {
        if (!xrtGetError()) xcbor_error(XERR_VALUE,
            "CBOR input must contain exactly one byte string");
        xrtValueRelease(pValue);
        return NULL;
    }
    pOutput = xrtBufferFrom(Data);
    xrtValueRelease(pValue);
    if (pOutput != NULL) { xrtClearError(); }
    return pOutput;
}

char* xcbor_to_hex(const void* pData, size_t iSize)
{
    str sHex;
    xrtClearError();
    if (pData == NULL && iSize != 0u) {
        xcbor_error(XERR_ARGUMENT, "CBOR data is missing");
        return NULL;
    }
    if (iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_RANGE, "CBOR data exceeds the 64 MiB safety limit");
        return NULL;
    }
    sHex = xrtHexEncodeNew((cbytes)pData, iSize, 0u);
    if (sHex != NULL) xrtClearError();
    return sHex;
}

xbuffer* xcbor_from_hex(const char* sText)
{
    size_t iLength = 0u;
    xrtClearError();
    if (sText == NULL) {
        xcbor_error(XERR_ARGUMENT, "CBOR hexadecimal text is missing");
        return NULL;
    }
    while (iLength <= 2u * XCBOR_MAX_BYTES && sText[iLength] != '\0')
        ++iLength;
    if (iLength > 2u * XCBOR_MAX_BYTES) {
        xcbor_error(XERR_RANGE, "CBOR hexadecimal text exceeds the safety limit");
        return NULL;
    }
    return xrtBufferFromHex((xstrview){ sText, iLength }, 0u);
}

const char* xcbor_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "CBOR operation failed";
}

bool xcbor_failed(void)
{
    return xrtGetError() != NULL;
}

/* XLang point 的 C ABI 是 void*；C 调用方继续使用上面的类型化接口。 */
void* xcbor_encode_abi(xvalue* value) { return xcbor_encode(value); }
xvalue* xcbor_decode_abi(void* data, size_t size) {
    xvalue* value = xcbor_decode(data, size);
    return value != NULL ? value : xrtValueNull();
}
void* xcbor_encode_bytes_abi(void* data, size_t size) {
    return xcbor_encode_bytes(data, size);
}
void* xcbor_decode_bytes_abi(void* data, size_t size) {
    return xcbor_decode_bytes(data, size);
}
void* xcbor_to_hex_abi(void* data, size_t size) {
    return xcbor_to_hex(data, size);
}
void* xcbor_from_hex_abi(const char* text) { return xcbor_from_hex(text); }
