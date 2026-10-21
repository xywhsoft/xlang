/* MessagePack 的 xvalue/xbuffer 绑定，不使用固定大小或共享结果缓冲。 */
#include <limits.h>
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
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "msgpack_native.h"

#define XMSGPACK_DOMAIN "xlang.extlib.msgpack"
#define XMSGPACK_MAX_DEPTH 64u
#define XMSGPACK_MAX_ITEMS 1000000u
#define XMSGPACK_MAX_BYTES (64u * 1024u * 1024u)

typedef struct xmsgpack_reader {
    const uint8* Data;
    size_t Size;
    size_t Offset;
    size_t Items;
} xmsgpack_reader;

static void xmsgpack_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XMSGPACK_DOMAIN, 0, sMessage);
}

static bool xmsgpack_append(xbuffer* pOutput, const void* pData, size_t iSize)
{
    if (iSize > XMSGPACK_MAX_BYTES || pOutput->Size > XMSGPACK_MAX_BYTES - iSize) {
        xmsgpack_error(XERR_RANGE, "MessagePack output exceeds the 64 MiB safety limit");
        return false;
    }
    return xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize });
}

static bool xmsgpack_u8(xbuffer* pOutput, uint8 iValue)
{
    return xmsgpack_append(pOutput, &iValue, 1u);
}

static bool xmsgpack_uint(xbuffer* pOutput, uint64 iValue, uint8 iBytes)
{
    uint8 aBytes[8];
    size_t i;
    for (i = 0u; i < iBytes; ++i) {
        aBytes[i] = (uint8)(iValue >> ((iBytes - 1u - i) * 8u));
    }
    return xmsgpack_append(pOutput, aBytes, iBytes);
}

static bool xmsgpack_unsigned(xbuffer* pOutput, uint64 iValue)
{
    if (iValue <= 127u) { return xmsgpack_u8(pOutput, (uint8)iValue); }
    if (iValue <= UINT8_MAX) { return xmsgpack_u8(pOutput, 0xccu) && xmsgpack_uint(pOutput, iValue, 1u); }
    if (iValue <= UINT16_MAX) { return xmsgpack_u8(pOutput, 0xcdu) && xmsgpack_uint(pOutput, iValue, 2u); }
    if (iValue <= UINT32_MAX) { return xmsgpack_u8(pOutput, 0xceu) && xmsgpack_uint(pOutput, iValue, 4u); }
    return xmsgpack_u8(pOutput, 0xcfu) && xmsgpack_uint(pOutput, iValue, 8u);
}

static bool xmsgpack_signed(xbuffer* pOutput, int64 iValue)
{
    if (iValue >= 0) { return xmsgpack_unsigned(pOutput, (uint64)iValue); }
    if (iValue >= -32) { return xmsgpack_u8(pOutput, (uint8)iValue); }
    if (iValue >= INT8_MIN) { return xmsgpack_u8(pOutput, 0xd0u) && xmsgpack_uint(pOutput, (uint8)iValue, 1u); }
    if (iValue >= INT16_MIN) { return xmsgpack_u8(pOutput, 0xd1u) && xmsgpack_uint(pOutput, (uint16)iValue, 2u); }
    if (iValue >= INT32_MIN) { return xmsgpack_u8(pOutput, 0xd2u) && xmsgpack_uint(pOutput, (uint32)iValue, 4u); }
    return xmsgpack_u8(pOutput, 0xd3u) && xmsgpack_uint(pOutput, (uint64)iValue, 8u);
}

static bool xmsgpack_time(xbuffer* pOutput, xtime Time)
{
    int64 iSeconds = Time / 1000000;
    int64 iMicros = Time % 1000000;
    uint64 iNanos;

    if (iMicros < 0) { --iSeconds; iMicros += 1000000; }
    iNanos = (uint64)iMicros * 1000u;
    if (iSeconds >= 0 && iSeconds <= UINT32_MAX && iNanos == 0u) {
        return xmsgpack_u8(pOutput, 0xd6u) && xmsgpack_u8(pOutput, 0xffu) &&
            xmsgpack_uint(pOutput, (uint64)iSeconds, 4u);
    }
    if (iSeconds >= 0 && (uint64)iSeconds < (UINT64_C(1) << 34u)) {
        return xmsgpack_u8(pOutput, 0xd7u) && xmsgpack_u8(pOutput, 0xffu) &&
            xmsgpack_uint(pOutput, (iNanos << 34u) | (uint64)iSeconds, 8u);
    }
    return xmsgpack_u8(pOutput, 0xc7u) && xmsgpack_u8(pOutput, 12u) &&
        xmsgpack_u8(pOutput, 0xffu) && xmsgpack_uint(pOutput, iNanos, 4u) &&
        xmsgpack_uint(pOutput, (uint64)iSeconds, 8u);
}

static bool xmsgpack_length(xbuffer* pOutput, uint8 iFixedBase, uint8 iFixedMax,
    uint8 i8, uint8 i16, uint8 i32, size_t iSize)
{
    if (iFixedBase != 0u && iSize <= iFixedMax) { return xmsgpack_u8(pOutput, (uint8)(iFixedBase | iSize)); }
    if (iSize <= UINT8_MAX && i8 != 0u) { return xmsgpack_u8(pOutput, i8) && xmsgpack_uint(pOutput, iSize, 1u); }
    if (iSize <= UINT16_MAX) { return xmsgpack_u8(pOutput, i16) && xmsgpack_uint(pOutput, iSize, 2u); }
    if (iSize <= UINT32_MAX) { return xmsgpack_u8(pOutput, i32) && xmsgpack_uint(pOutput, iSize, 4u); }
    xmsgpack_error(XERR_RANGE, "MessagePack collection exceeds uint32 length");
    return false;
}

static bool xmsgpack_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems);

static bool xmsgpack_array(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    size_t iCount = xrtValueCount(pValue);
    size_t i;
    if (iCount > XMSGPACK_MAX_ITEMS - *pItems) {
        xmsgpack_error(XERR_RANGE, "MessagePack document has too many array items");
        return false;
    }
    *pItems += iCount;
    if (!xmsgpack_length(pOutput, 0x90u, 15u, 0u, 0xdcu, 0xddu, iCount)) { return false; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pItem = xrtValueArrayGet(pValue, i);
        if (pItem == NULL || !xmsgpack_value(pOutput, pItem, iDepth + 1u, pItems)) { return false; }
    }
    return true;
}

static bool xmsgpack_object(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    size_t iCount = xrtValueCount(pValue);
    size_t i;
    if (iCount > XMSGPACK_MAX_ITEMS - *pItems) {
        xmsgpack_error(XERR_RANGE, "MessagePack document has too many map items");
        return false;
    }
    *pItems += iCount;
    if (!xmsgpack_length(pOutput, 0x80u, 15u, 0u, 0xdeu, 0xdfu, iCount)) { return false; }
    for (i = 0u; i < iCount; ++i) {
        xstrview Key;
        xvalue* pItem = xrtValueObjectAt(pValue, i, &Key);
        if (pItem == NULL || !xrtUtf8Valid(Key, NULL)) {
            xmsgpack_error(XERR_VALUE, "MessagePack map key must be UTF-8 text");
            return false;
        }
        if (!xmsgpack_length(pOutput, 0xa0u, 31u, 0xd9u, 0xdau, 0xdbu, Key.Size) ||
            !xmsgpack_append(pOutput, Key.Data, Key.Size) || !xmsgpack_value(pOutput, pItem, iDepth + 1u, pItems)) { return false; }
    }
    return true;
}

static bool xmsgpack_value(xbuffer* pOutput, const xvalue* pValue, size_t iDepth, size_t* pItems)
{
    xvaluetype Type;
    bool bValue;
    int64 iValue;
    uint64 iUnsigned;
    double fValue;
    xtime Time;
    uint64 iBits;
    xstrview Text;
    xbytesview Bytes;

    if (pValue == NULL) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack value is missing");
        return false;
    }
    if (iDepth > XMSGPACK_MAX_DEPTH) {
        xmsgpack_error(XERR_RANGE, "MessagePack nesting exceeds the depth limit");
        return false;
    }
    Type = xrtValueType(pValue);
    switch (Type) {
        case XVALUE_NULL: return xmsgpack_u8(pOutput, 0xc0u);
        case XVALUE_BOOL: return xrtValueGetBool(pValue, &bValue) && xmsgpack_u8(pOutput, bValue ? 0xc3u : 0xc2u);
        case XVALUE_INT: return xrtValueGetInt(pValue, &iValue) && xmsgpack_signed(pOutput, iValue);
        case XVALUE_UINT: return xrtValueGetUInt(pValue, &iUnsigned) && xmsgpack_unsigned(pOutput, iUnsigned);
        case XVALUE_TIME: return xrtValueGetTime(pValue, &Time) && xmsgpack_time(pOutput, Time);
        case XVALUE_FLOAT:
            if (!xrtValueGetFloat(pValue, &fValue)) { return false; }
            memcpy(&iBits, &fValue, sizeof(iBits));
            return xmsgpack_u8(pOutput, 0xcbu) && xmsgpack_uint(pOutput, iBits, 8u);
        case XVALUE_STRING:
            if (!xrtValueGetString(pValue, &Text) || !xrtUtf8Valid(Text, NULL)) {
                xmsgpack_error(XERR_VALUE, "MessagePack text must be valid UTF-8");
                return false;
            }
            return xmsgpack_length(pOutput, 0xa0u, 31u, 0xd9u, 0xdau, 0xdbu, Text.Size) &&
                xmsgpack_append(pOutput, Text.Data, Text.Size);
        case XVALUE_BYTES:
            return xrtValueGetBytes(pValue, &Bytes) && xmsgpack_length(pOutput, 0u, 0u, 0xc4u, 0xc5u, 0xc6u, Bytes.Size) &&
                xmsgpack_append(pOutput, Bytes.Data, Bytes.Size);
        case XVALUE_ARRAY: return xmsgpack_array(pOutput, pValue, iDepth, pItems);
        case XVALUE_OBJECT: return xmsgpack_object(pOutput, pValue, iDepth, pItems);
        default:
            xmsgpack_error(XERR_UNSUPPORTED, "MessagePack cannot encode this dynamic value type");
            return false;
    }
}

static bool xmsgpack_need(xmsgpack_reader* pReader, size_t iSize)
{
    if (iSize > pReader->Size - pReader->Offset) { xmsgpack_error(XERR_VALUE, "truncated MessagePack input"); return false; }
    return true;
}

static bool xmsgpack_read_uint(xmsgpack_reader* pReader, uint8 iBytes, uint64* pValue)
{
    uint64 iValue = 0u;
    size_t i;
    if (!xmsgpack_need(pReader, iBytes)) { return false; }
    for (i = 0u; i < iBytes; ++i) { iValue = (iValue << 8u) | pReader->Data[pReader->Offset++]; }
    *pValue = iValue;
    return true;
}

static xvalue* xmsgpack_read_time(xmsgpack_reader* pReader, uint8 iHead)
{
    uint64 iType, iBits, iNanos = 0u;
    int64 iSeconds;
    int64 iMicros;

    if (iHead == 0xc7u && (!xmsgpack_read_uint(pReader, 1u, &iBits) || iBits != 12u)) {
        if (!xrtGetError()) xmsgpack_error(XERR_UNSUPPORTED, "unsupported MessagePack extension length");
        return NULL;
    }
    if (!xmsgpack_read_uint(pReader, 1u, &iType)) { return NULL; }
    if (iType != 0xffu) {
        xmsgpack_error(XERR_UNSUPPORTED, "unsupported MessagePack extension type");
        return NULL;
    }
    if (iHead == 0xd6u) {
        if (!xmsgpack_read_uint(pReader, 4u, &iBits)) { return NULL; }
        iSeconds = (int64)iBits;
    } else if (iHead == 0xd7u) {
        if (!xmsgpack_read_uint(pReader, 8u, &iBits)) { return NULL; }
        iNanos = iBits >> 34u;
        iSeconds = (int64)(iBits & ((UINT64_C(1) << 34u) - 1u));
    } else {
        if (!xmsgpack_read_uint(pReader, 4u, &iNanos) ||
            !xmsgpack_read_uint(pReader, 8u, &iBits)) { return NULL; }
        iSeconds = iBits <= INT64_MAX ? (int64)iBits :
            -1 - (int64)(UINT64_MAX - iBits);
    }
    if (iNanos >= 1000000000u || iNanos % 1000u != 0u) {
        xmsgpack_error(XERR_VALUE, "MessagePack timestamp is invalid or finer than microseconds");
        return NULL;
    }
    iMicros = (int64)(iNanos / 1000u);
    if (iSeconds < INT64_MIN / 1000000 - 1 || iSeconds > INT64_MAX / 1000000 ||
        (iSeconds == INT64_MIN / 1000000 - 1 && iMicros < 1000000 + INT64_MIN % 1000000) ||
        (iSeconds == INT64_MAX / 1000000 && iMicros > INT64_MAX % 1000000)) {
        xmsgpack_error(XERR_RANGE, "MessagePack timestamp exceeds XRT time range");
        return NULL;
    }
    if (iSeconds == INT64_MIN / 1000000 - 1) {
        return xrtValueTime((iSeconds + 1) * 1000000 - (1000000 - iMicros));
    }
    return xrtValueTime(iSeconds * 1000000 + iMicros);
}

static xvalue* xmsgpack_read_value(xmsgpack_reader* pReader, size_t iDepth);

static xvalue* xmsgpack_read_array(xmsgpack_reader* pReader, uint64 iCount, size_t iDepth)
{
    uint64 i;
    xvalue* pResult;
    if (iCount > XMSGPACK_MAX_ITEMS || pReader->Items > XMSGPACK_MAX_ITEMS - (size_t)iCount) { xmsgpack_error(XERR_RANGE, "MessagePack array has too many items"); return NULL; }
    pReader->Items += (size_t)iCount;
    pResult = xrtValueArray();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pItem = xmsgpack_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueArrayAppendTake(pResult, &pItem)) { xrtValueRelease(pItem); xrtValueRelease(pResult); return NULL; }
    }
    return pResult;
}

static xvalue* xmsgpack_read_object(xmsgpack_reader* pReader, uint64 iCount, size_t iDepth)
{
    uint64 i;
    xvalue* pResult;
    if (iCount > XMSGPACK_MAX_ITEMS || pReader->Items > XMSGPACK_MAX_ITEMS - (size_t)iCount) { xmsgpack_error(XERR_RANGE, "MessagePack map has too many items"); return NULL; }
    pReader->Items += (size_t)iCount;
    pResult = xrtValueObject();
    if (pResult == NULL) { return NULL; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pKey = xmsgpack_read_value(pReader, iDepth + 1u);
        xvalue* pItem;
        xstrview Key;
        if (pKey == NULL) { xrtValueRelease(pResult); return NULL; }
        if (!xrtValueGetString(pKey, &Key)) { xrtValueRelease(pKey); xrtValueRelease(pResult); xmsgpack_error(XERR_VALUE, "MessagePack map keys must be UTF-8 strings"); return NULL; }
        if (xrtValueObjectHas(pResult, Key)) {
            xrtValueRelease(pKey);
            xrtValueRelease(pResult);
            xmsgpack_error(XERR_VALUE, "duplicate MessagePack map key");
            return NULL;
        }
        pItem = xmsgpack_read_value(pReader, iDepth + 1u);
        if (pItem == NULL || !xrtValueObjectSetTake(pResult, Key, &pItem)) { xrtValueRelease(pKey); xrtValueRelease(pItem); xrtValueRelease(pResult); return NULL; }
        xrtValueRelease(pKey);
    }
    return pResult;
}

static xvalue* xmsgpack_read_blob(xmsgpack_reader* pReader, uint64 iSize, bool bText)
{
    xvalue* pResult;
    if (iSize > XMSGPACK_MAX_BYTES || iSize > SIZE_MAX) {
        xmsgpack_error(XERR_RANGE, "MessagePack string exceeds the 64 MiB safety limit");
        return NULL;
    }
    if (!xmsgpack_need(pReader, (size_t)iSize)) { return NULL; }
    if (bText && !xrtUtf8Valid((xstrview){ (cstr)(pReader->Data + pReader->Offset), (size_t)iSize }, NULL)) { xmsgpack_error(XERR_VALUE, "MessagePack text is not valid UTF-8"); return NULL; }
    pResult = bText ? xrtValueString((xstrview){ (cstr)(pReader->Data + pReader->Offset), (size_t)iSize }) :
        xrtValueBytes((xbytesview){ pReader->Data + pReader->Offset, (size_t)iSize });
    pReader->Offset += (size_t)iSize;
    return pResult;
}

static xvalue* xmsgpack_read_value(xmsgpack_reader* pReader, size_t iDepth)
{
    uint8 iHead;
    uint64 iValue;
    if (iDepth > XMSGPACK_MAX_DEPTH) {
        xmsgpack_error(XERR_RANGE, "MessagePack nesting depth exceeds the safety limit");
        return NULL;
    }
    if (!xmsgpack_need(pReader, 1u)) { return NULL; }
    iHead = pReader->Data[pReader->Offset++];
    if (iHead <= 0x7fu) { return xrtValueInt(iHead); }
    if (iHead >= 0xe0u) { return xrtValueInt((int8)iHead); }
    if ((iHead & 0xe0u) == 0xa0u) { return xmsgpack_read_blob(pReader, iHead & 31u, true); }
    if ((iHead & 0xf0u) == 0x90u) { return xmsgpack_read_array(pReader, iHead & 15u, iDepth); }
    if ((iHead & 0xf0u) == 0x80u) { return xmsgpack_read_object(pReader, iHead & 15u, iDepth); }
    switch (iHead) {
        case 0xc0u: return xrtValueRetain(xrtValueNull()); case 0xc2u: return xrtValueRetain(xrtValueBool(false)); case 0xc3u: return xrtValueRetain(xrtValueBool(true));
        case 0xccu: if (!xmsgpack_read_uint(pReader, 1u, &iValue)) return NULL; return xrtValueInt((int64)iValue);
        case 0xcdu: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xrtValueInt((int64)iValue);
        case 0xceu: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xrtValueInt((int64)iValue);
        case 0xcfu:
            if (!xmsgpack_read_uint(pReader, 8u, &iValue)) return NULL;
            if (iValue > INT64_MAX) return xrtValueUInt(iValue);
            return xrtValueInt((int64)iValue);
        case 0xd0u: if (!xmsgpack_read_uint(pReader, 1u, &iValue)) return NULL; return xrtValueInt((int8)iValue);
        case 0xd1u: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xrtValueInt((int16)iValue);
        case 0xd2u: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xrtValueInt((int32)iValue);
        case 0xd3u: if (!xmsgpack_read_uint(pReader, 8u, &iValue)) return NULL; return xrtValueInt((int64)iValue);
        case 0xcau: { uint32 iBits; float fValue; if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; iBits = (uint32)iValue; memcpy(&fValue, &iBits, sizeof(fValue)); return xrtValueFloat((double)fValue); }
        case 0xcbu: { double fValue; if (!xmsgpack_read_uint(pReader, 8u, &iValue)) return NULL; memcpy(&fValue, &iValue, sizeof(fValue)); return xrtValueFloat(fValue); }
        case 0xd6u: case 0xd7u: case 0xc7u:
            return xmsgpack_read_time(pReader, iHead);
        case 0xd9u: if (!xmsgpack_read_uint(pReader, 1u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, true);
        case 0xdau: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, true);
        case 0xdbu: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, true);
        case 0xc4u: if (!xmsgpack_read_uint(pReader, 1u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, false);
        case 0xc5u: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, false);
        case 0xc6u: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xmsgpack_read_blob(pReader, iValue, false);
        case 0xdcu: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xmsgpack_read_array(pReader, iValue, iDepth);
        case 0xddu: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xmsgpack_read_array(pReader, iValue, iDepth);
        case 0xdeu: if (!xmsgpack_read_uint(pReader, 2u, &iValue)) return NULL; return xmsgpack_read_object(pReader, iValue, iDepth);
        case 0xdfu: if (!xmsgpack_read_uint(pReader, 4u, &iValue)) return NULL; return xmsgpack_read_object(pReader, iValue, iDepth);
        default: xmsgpack_error(XERR_UNSUPPORTED, "unsupported MessagePack value type"); return NULL;
    }
}

xbuffer* xmsgpack_encode(xvalue* pValue)
{
    size_t iItems = 0u;
    xbuffer* pOutput;
    xrtClearError();
    if (pValue == NULL) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack value is missing");
        return NULL;
    }
    pOutput = xrtBufferCreate();
    if (pOutput == NULL || !xmsgpack_value(pOutput, pValue, 0u, &iItems)) { xrtBufferDestroy(pOutput); return NULL; }
    xrtClearError();
    return pOutput;
}

xvalue* xmsgpack_decode(const void* pData, size_t iSize)
{
    xmsgpack_reader Reader = { (const uint8*)pData, iSize, 0u, 0u };
    xvalue* pValue;
    xrtClearError();
    if ((pData == NULL && iSize != 0u) || iSize == 0u) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack input is missing");
        return NULL;
    }
    if (iSize > XMSGPACK_MAX_BYTES) {
        xmsgpack_error(XERR_RANGE, "MessagePack input exceeds the 64 MiB safety limit");
        return NULL;
    }
    pValue = xmsgpack_read_value(&Reader, 0u);
    if (pValue == NULL || Reader.Offset != Reader.Size) { xrtValueRelease(pValue); if (pValue != NULL) xmsgpack_error(XERR_VALUE, "trailing bytes after MessagePack value"); return NULL; }
    xrtClearError();
    return pValue;
}

xbuffer* xmsgpack_encode_bytes(const void* pData, size_t iSize)
{
    xvalue* pValue;
    xbuffer* pResult;
    if (pData == NULL && iSize != 0u) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack byte input is missing");
        return NULL;
    }
    if (iSize > XMSGPACK_MAX_BYTES) {
        xmsgpack_error(XERR_RANGE, "MessagePack byte input exceeds the 64 MiB safety limit");
        return NULL;
    }
    pValue = xrtValueBytes((xbytesview){ (cbytes)pData, iSize });
    if (pValue == NULL) { return NULL; }
    pResult = xmsgpack_encode(pValue);
    xrtValueRelease(pValue);
    return pResult;
}

xbuffer* xmsgpack_decode_bytes(const void* pData, size_t iSize)
{
    xvalue* pValue = xmsgpack_decode(pData, iSize);
    xbytesview Bytes;
    xbuffer* pResult = NULL;
    if (pValue == NULL) { return NULL; }
    if (!xrtValueGetBytes(pValue, &Bytes)) { xrtValueRelease(pValue); xmsgpack_error(XERR_VALUE, "MessagePack input must contain exactly one byte string"); return NULL; }
    pResult = xrtBufferFrom(Bytes);
    xrtValueRelease(pValue);
    return pResult;
}

str xmsgpack_to_hex(const void* pData, size_t iSize)
{
    xrtClearError();
    if (pData == NULL && iSize != 0u) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack data is missing");
        return NULL;
    }
    if (iSize > XMSGPACK_MAX_BYTES) {
        xmsgpack_error(XERR_RANGE, "MessagePack data exceeds the 64 MiB safety limit");
        return NULL;
    }
    return xrtHexEncodeNew((cbytes)pData, iSize, 0u);
}

xbuffer* xmsgpack_from_hex(cstr sText)
{
    size_t iSize;
    xrtClearError();
    if (sText == NULL) {
        xmsgpack_error(XERR_ARGUMENT, "MessagePack hex text is missing");
        return NULL;
    }
    iSize = 0u;
    while (iSize <= 2u * XMSGPACK_MAX_BYTES && sText[iSize] != '\0')
        ++iSize;
    if (iSize > 2u * XMSGPACK_MAX_BYTES) {
        xmsgpack_error(XERR_RANGE, "MessagePack hex text exceeds the 128 MiB safety limit");
        return NULL;
    }
    return xrtBufferFromHex((xstrview){ sText, iSize }, 0u);
}

cstr xmsgpack_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "MessagePack operation failed";
}

bool xmsgpack_has_error(void) { return xrtGetError() != NULL; }

void* xmsgpack_encode_abi(xvalue* value) { return xmsgpack_encode(value); }
xvalue* xmsgpack_decode_abi(void* data, size_t size) {
    xvalue* value = xmsgpack_decode(data, size);
    return value != NULL ? value : xrtValueNull();
}
void* xmsgpack_encode_bytes_abi(void* data, size_t size) {
    return xmsgpack_encode_bytes(data, size);
}
void* xmsgpack_decode_bytes_abi(void* data, size_t size) {
    return xmsgpack_decode_bytes(data, size);
}
void* xmsgpack_to_hex_abi(void* data, size_t size) {
    return xmsgpack_to_hex(data, size);
}
void* xmsgpack_from_hex_abi(cstr text) { return xmsgpack_from_hex(text); }
