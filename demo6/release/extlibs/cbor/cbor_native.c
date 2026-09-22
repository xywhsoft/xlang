/* CBOR RFC 8949 的 xvalue/xbuffer 绑定。 */
#include <limits.h>
#include <math.h>
#include <string.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_CODEC_HEX
#define XRT_MODULE_TIME_TEXT
#define XRT_MODULE_UNICODE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#if defined(_WIN32) || defined(_WIN64)
#define XCBOR_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XCBOR_THREAD __thread
#else
#define XCBOR_THREAD
#endif

#define XCBOR_DOMAIN "xlang.extlib.cbor"
#define XCBOR_MAX_DEPTH 64u
#define XCBOR_MAX_ITEMS 1000000u
#define XCBOR_MAX_BYTES (64u * 1024u * 1024u)
#define XCBOR_RESULT_SLOTS 8u

typedef struct xcborreader {
    const uint8* Data;
    size_t Size;
    size_t Offset;
    size_t Items;
} xcborreader;

static XCBOR_THREAD xbuffer g_xcbor_results[XCBOR_RESULT_SLOTS];
static XCBOR_THREAD size_t g_xcbor_result_index;

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
    return xcbor_head(pOutput, 3u, (uint64)Text.Size) &&
        xcbor_append(pOutput, Text.Data, Text.Size);
}

static bool xcbor_bytes(xbuffer* pOutput, xbytesview Data)
{
    return xcbor_head(pOutput, 2u, (uint64)Data.Size) &&
        xcbor_append(pOutput, Data.Data, Data.Size);
}

static bool xcbor_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth);

static bool xcbor_array(xbuffer* pOutput, const xvalue* pValue, size_t iDepth)
{
    size_t i;
    size_t iCount = xrtValueCount(pValue);

    if (iCount > XCBOR_MAX_ITEMS || !xcbor_head(pOutput, 4u, (uint64)iCount)) {
        return false;
    }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pItem = xrtValueArrayGet(pValue, i);
        if (pItem == NULL || !xcbor_value(pOutput, pItem, iDepth + 1u)) {
            if (pItem == NULL) {
                xcbor_error(XERR_VALUE, "CBOR array contains an invalid value");
            }
            return false;
        }
    }
    return true;
}

static bool xcbor_object(xbuffer* pOutput, const xvalue* pValue, size_t iDepth)
{
    size_t i;
    size_t iCount = xrtValueCount(pValue);

    if (iCount > XCBOR_MAX_ITEMS || !xcbor_head(pOutput, 5u, (uint64)iCount)) {
        return false;
    }
    for (i = 0u; i < iCount; ++i) {
        xstrview Key;
        xvalue* pItem = xrtValueObjectAt(pValue, i, &Key);
        if (pItem == NULL || !xcbor_text(pOutput, Key) ||
            !xcbor_value(pOutput, pItem, iDepth + 1u)) {
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

static bool xcbor_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth)
{
    xvaluetype Type;
    bool bValue;
    int64 iValue;
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
            return xrtValueGetTime(pValue, &Time) && xcbor_time(pOutput, Time);
        case XVALUE_ARRAY:
            return xcbor_array(pOutput, pValue, iDepth);
        case XVALUE_OBJECT:
            return xcbor_object(pOutput, pValue, iDepth);
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
        xcbor_error(XERR_PROTOCOL, "truncated CBOR input");
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
    xcbor_error(XERR_PROTOCOL, "indefinite or reserved CBOR lengths are not supported");
    return false;
}

static xvalue* xcbor_read_value(xcborreader* pReader, size_t iDepth);

static xvalue* xcbor_read_array(xcborreader* pReader, uint64 iCount, size_t iDepth)
{
    xvalue* pResult;
    uint64 i;

    if (iCount > XCBOR_MAX_ITEMS || pReader->Items > XCBOR_MAX_ITEMS - (size_t)iCount) {
        xcbor_error(XERR_RANGE, "CBOR array has too many items");
        return NULL;
    }
    pReader->Items += (size_t)iCount;
    pResult = xrtValueArray();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pItem = xcbor_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueArrayAppendTake(pResult, &pItem)) {
            xrtValueRelease(pItem);
            xrtValueRelease(pResult);
            return NULL;
        }
    }
    return pResult;
}

static xvalue* xcbor_read_object(xcborreader* pReader, uint64 iCount, size_t iDepth)
{
    xvalue* pResult;
    uint64 i;

    if (iCount > XCBOR_MAX_ITEMS || pReader->Items > XCBOR_MAX_ITEMS - (size_t)iCount) {
        xcbor_error(XERR_RANGE, "CBOR map has too many items");
        return NULL;
    }
    pReader->Items += (size_t)iCount;
    pResult = xrtValueObject();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; i < iCount; ++i) {
        uint8 iMajor;
        uint8 iInfo;
        uint64 iSize;
        xstrview Key;
        xvalue* pItem;

        if (!xcbor_head_read(pReader, &iMajor, &iInfo, &iSize) || iMajor != 3u ||
            iSize > SIZE_MAX || !xcbor_need(pReader, (size_t)iSize) ||
            !xrtUtf8Valid((xstrview){ (cstr)(pReader->Data + pReader->Offset), (size_t)iSize }, NULL)) {
            if (xrtGetError() == NULL) {
                xcbor_error(XERR_PROTOCOL, "CBOR map keys must be valid UTF-8 text");
            }
            xrtValueRelease(pResult);
            return NULL;
        }
        Key = (xstrview){ (cstr)(pReader->Data + pReader->Offset), (size_t)iSize };
        pReader->Offset += (size_t)iSize;
        pItem = xcbor_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueObjectSetTake(pResult, Key, &pItem)) {
            xrtValueRelease(pItem);
            xrtValueRelease(pResult);
            return NULL;
        }
    }
    return pResult;
}

static xvalue* xcbor_read_tag(xcborreader* pReader, uint64 iTag, size_t iDepth)
{
    xvalue* pValue = xcbor_read_value(pReader, iDepth + 1u);
    xtime Time;

    if (pValue == NULL) { return NULL; }
    if (iTag == 0u) {
        xstrview Text;
        if (!xrtValueGetString(pValue, &Text) || !xrtTimeParseRFC3339(Text, &Time)) {
            xrtValueRelease(pValue);
            xcbor_error(XERR_PROTOCOL, "CBOR tag 0 must contain RFC 3339 text");
            return NULL;
        }
        xrtValueRelease(pValue);
        return xrtValueTime(Time);
    }
    if (iTag == 1u) {
        int64 iSeconds;
        double fSeconds;
        if (xrtValueGetInt(pValue, &iSeconds)) {
            xrtValueRelease(pValue);
            if (!xrtTimeFromUnix(iSeconds, &Time)) { return NULL; }
            return xrtValueTime(Time);
        }
        if (xrtValueGetFloat(pValue, &fSeconds) && isfinite(fSeconds) &&
            fSeconds >= (double)INT64_MIN / 1000000.0 &&
            fSeconds <= (double)INT64_MAX / 1000000.0) {
            xrtValueRelease(pValue);
            return xrtValueTime((xtime)llround(fSeconds * 1000000.0));
        }
        xrtValueRelease(pValue);
        xcbor_error(XERR_PROTOCOL, "CBOR tag 1 must contain epoch seconds");
        return NULL;
    }
    xrtValueRelease(pValue);
    xcbor_error(XERR_UNSUPPORTED, "unsupported CBOR semantic tag");
    return NULL;
}

static xvalue* xcbor_read_value(xcborreader* pReader, size_t iDepth)
{
    uint8 iMajor;
    uint64 iValue;

    if (iDepth > XCBOR_MAX_DEPTH) {
        xcbor_error(XERR_RANGE, "CBOR nesting exceeds the depth limit");
        return NULL;
    }
    {
        uint8 iInfo;
        if (!xcbor_head_read(pReader, &iMajor, &iInfo, &iValue)) { return NULL; }
        if (iMajor == 7u) {
            if (iInfo == 20u) { return xrtValueRetain(xrtValueBool(false)); }
            if (iInfo == 21u) { return xrtValueRetain(xrtValueBool(true)); }
            if (iInfo == 22u) { return xrtValueRetain(xrtValueNull()); }
            if (iInfo == 27u) {
                uint64 iBits = iValue;
                double fValue;
                memcpy(&fValue, &iBits, sizeof(fValue));
                return xrtValueFloat(fValue);
            }
            xcbor_error(XERR_UNSUPPORTED, "unsupported CBOR simple or floating-point value");
            return NULL;
        }
    }
    switch (iMajor) {
        case 0u:
            if (iValue > INT64_MAX) { xcbor_error(XERR_RANGE, "CBOR unsigned integer exceeds int64"); return NULL; }
            return xrtValueInt((int64)iValue);
        case 1u:
            if (iValue > INT64_MAX) { xcbor_error(XERR_RANGE, "CBOR negative integer exceeds int64"); return NULL; }
            return xrtValueInt(-1 - (int64)iValue);
        case 2u:
        case 3u: {
            xvalue* pResult;
            if (iValue > SIZE_MAX || !xcbor_need(pReader, (size_t)iValue)) { return NULL; }
            if (iMajor == 3u && !xrtUtf8Valid((xstrview){
                (cstr)(pReader->Data + pReader->Offset), (size_t)iValue
            }, NULL)) {
                xcbor_error(XERR_PROTOCOL, "CBOR text is not valid UTF-8");
                return NULL;
            }
            pResult = iMajor == 2u ?
                xrtValueBytes((xbytesview){ pReader->Data + pReader->Offset, (size_t)iValue }) :
                xrtValueString((xstrview){
                    (cstr)(pReader->Data + pReader->Offset), (size_t)iValue
                });
            pReader->Offset += (size_t)iValue;
            return pResult;
        }
        case 4u:
            return xcbor_read_array(pReader, iValue, iDepth);
        case 5u:
            return xcbor_read_object(pReader, iValue, iDepth);
        case 6u:
            return xcbor_read_tag(pReader, iValue, iDepth);
        default:
            xcbor_error(XERR_PROTOCOL, "invalid CBOR major type");
            return NULL;
    }
}

static xbuffer* xcbor_encode_value(const xvalue* pValue)
{
    xbuffer* pOutput = xrtBufferCreate();
    if (pOutput == NULL) { return NULL; }
    if (!xcbor_value(pOutput, pValue, 0u)) {
        xrtBufferDestroy(pOutput);
        return NULL;
    }
    xrtClearError();
    return pOutput;
}

xbuffer* xcbor_encode(xvalue* pValue)
{
    return xcbor_encode_value(pValue);
}

xvalue* xcbor_decode(const void* pData, size_t iSize)
{
    xcborreader Reader;
    xvalue* pValue;

    if ((pData == NULL && iSize != 0u) || iSize == 0u || iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_ARGUMENT, "CBOR input must contain at most 64 MiB of data");
        return xrtValueNull();
    }
    Reader = (xcborreader){ (const uint8*)pData, iSize, 0u, 0u };
    pValue = xcbor_read_value(&Reader, 0u);
    if (pValue == NULL || Reader.Offset != Reader.Size) {
        xrtValueRelease(pValue);
        if (pValue != NULL && Reader.Offset != Reader.Size) {
            xcbor_error(XERR_PROTOCOL, "trailing bytes after the CBOR item");
        }
        return xrtValueNull();
    }
    xrtClearError();
    return pValue;
}

xbuffer* xcbor_encode_bytes(const void* pData, size_t iSize)
{
    xbuffer* pOutput;
    if ((pData == NULL && iSize != 0u) || iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_ARGUMENT, "CBOR byte input must contain at most 64 MiB of data");
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
    xcborreader Reader;
    uint8 iMajor;
    uint64 iLength;
    xbuffer* pOutput;

    if ((pData == NULL && iSize != 0u) || iSize == 0u || iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_ARGUMENT, "CBOR input must contain at most 64 MiB of data");
        return NULL;
    }
    Reader = (xcborreader){ (const uint8*)pData, iSize, 0u, 0u };
    {
        uint8 iInfo;
        if (!xcbor_head_read(&Reader, &iMajor, &iInfo, &iLength) || iMajor != 2u ||
        iLength > SIZE_MAX || !xcbor_need(&Reader, (size_t)iLength) ||
        Reader.Offset + (size_t)iLength != Reader.Size) {
            if (xrtGetError() == NULL) {
                xcbor_error(XERR_PROTOCOL, "CBOR input must contain exactly one byte string");
            }
            return NULL;
        }
    }
    pOutput = xrtBufferFrom((xbytesview){ Reader.Data + Reader.Offset, (size_t)iLength });
    if (pOutput != NULL) { xrtClearError(); }
    return pOutput;
}

const char* xcbor_to_hex(const void* pData, size_t iSize)
{
    xbuffer* pSlot;
    str sHex;
    size_t iSlot;

    if ((pData == NULL && iSize != 0u) || iSize > XCBOR_MAX_BYTES) {
        xcbor_error(XERR_ARGUMENT, "CBOR data must contain at most 64 MiB of data");
        return "";
    }
    sHex = xrtHexEncodeNew((cbytes)pData, iSize, 0u);
    if (sHex == NULL) { return ""; }
    iSlot = g_xcbor_result_index++ % XCBOR_RESULT_SLOTS;
    pSlot = &g_xcbor_results[iSlot];
    xrtBufferUnit(pSlot);
    if (!xrtBufferSetTake(pSlot, (bytes*)&sHex, strlen(sHex), strlen(sHex) + 1u)) {
        xrtFree(sHex);
        xcbor_error(XERR_MEMORY, "could not store CBOR hexadecimal text");
        return "";
    }
    xrtClearError();
    return (const char*)pSlot->Data;
}

xbuffer* xcbor_from_hex(const char* sText)
{
    if (sText == NULL) {
        xcbor_error(XERR_ARGUMENT, "CBOR hexadecimal text is missing");
        return NULL;
    }
    return xrtBufferFromHex(xrtStrView(sText), 0u);
}

const char* xcbor_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "CBOR operation failed";
}
