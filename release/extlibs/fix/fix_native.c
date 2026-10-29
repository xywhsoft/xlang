/* FIX 4.x message codec. Session, reconnect and venue dictionaries stay outside this package. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#include "xrt.h"
#include "fix_native.h"

#define XFIX_DOMAIN "xlang.extlib.fix"
#define XFIX_MAX_FIELDS 4096u
#define XFIX_MAX_BYTES (16u * 1024u * 1024u)

static void xfix_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XFIX_DOMAIN, 0, sMessage);
}

/* C 字符串入口在查找终止符时也受单帧上限约束。 */
static bool xfix_text_size(cstr sText, bool bRejectSoh, size_t* pSize)
{
    size_t i;
    for (i = 0u; i <= XFIX_MAX_BYTES; ++i) {
        if (sText[i] == '\0') { *pSize = i; return true; }
        if (bRejectSoh && sText[i] == '\x01') {
            xfix_error(XERR_VALUE, "FIX field value cannot contain SOH");
            return false;
        }
    }
    xfix_error(XERR_RANGE, "FIX text exceeds the 16 MiB safety limit");
    return false;
}

static bool xfix_append(xbuffer* pOutput, const void* pData, size_t iSize)
{
    if (iSize > XFIX_MAX_BYTES || pOutput->Size > XFIX_MAX_BYTES - iSize) {
        xfix_error(XERR_RANGE, "FIX message exceeds the 16 MiB safety limit");
        return false;
    }
    return xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize });
}

static bool xfix_field_text(xbuffer* pOutput, int64 iTag, xstrview Value)
{
    char aTag[32];
    int iLength;
    const char iSoh = '\x01';
    if (Value.Size > XFIX_MAX_BYTES) {
        xfix_error(XERR_RANGE, "FIX field value exceeds the 16 MiB safety limit");
        return false;
    }
    if (iTag <= 0 ||
        (Value.Size > 0 && (!Value.Data || memchr(Value.Data, '\x01', Value.Size) != NULL ||
            memchr(Value.Data, '\0', Value.Size) != NULL))) {
        xfix_error(XERR_VALUE, "FIX fields need a positive tag and a value without SOH or NUL");
        return false;
    }
    iLength = snprintf(aTag, sizeof(aTag), "%lld=", (long long)iTag);
    return iLength > 0 && (size_t)iLength < sizeof(aTag) &&
        xfix_append(pOutput, aTag, (size_t)iLength) &&
        xfix_append(pOutput, Value.Data, Value.Size) &&
        xfix_append(pOutput, &iSoh, 1u);
}

static bool xfix_value_text(const xvalue* pValue, char* aScratch, size_t iCapacity, xstrview* pText)
{
    int64 iInt;
    uint64 iUInt;
    double fFloat;
    bool bBool;
    if (pValue == NULL) { return false; }
    /* XRT Get 是精确读取；不要用失败调用探测类型并留下错误状态。 */
    switch (xrtValueType(pValue)) {
        case XVALUE_STRING:
            return xrtValueGetString(pValue, pText);
        case XVALUE_INT: {
            int iLength;
            if (!xrtValueGetInt(pValue, &iInt)) return false;
            iLength = snprintf(aScratch, iCapacity, "%lld", (long long)iInt);
            if (iLength < 0 || (size_t)iLength >= iCapacity) return false;
            *pText = (xstrview){ aScratch, (size_t)iLength };
            return true;
        }
        case XVALUE_UINT: {
            int iLength;
            if (!xrtValueGetUInt(pValue, &iUInt)) return false;
            iLength = snprintf(aScratch, iCapacity, "%llu", (unsigned long long)iUInt);
            if (iLength < 0 || (size_t)iLength >= iCapacity) return false;
            *pText = (xstrview){ aScratch, (size_t)iLength };
            return true;
        }
        case XVALUE_FLOAT: {
            size_t iLength;
            if (!xrtValueGetFloat(pValue, &fFloat)) return false;
            if (!xrtNumWrite(fFloat, aScratch, iCapacity, &iLength,
                XNUMBER_FLOAT_COMPACT)) return false;
            *pText = (xstrview){ aScratch, iLength };
            return true;
        }
        case XVALUE_BOOL:
            if (!xrtValueGetBool(pValue, &bBool)) return false;
            *pText = bBool ? XRT_STR_LITERAL("Y") : XRT_STR_LITERAL("N");
            return true;
        default:
            xfix_error(XERR_VALUE, "FIX field values must be string, int, float or bool");
            return false;
    }
}

static unsigned int xfix_checksum_bytes(const uint8* pData, size_t iSize)
{
    size_t i;
    unsigned int iSum = 0u;
    for (i = 0u; i < iSize; ++i) { iSum = (iSum + pData[i]) & 0xffu; }
    return iSum;
}

/* Builds 8=BeginString, 9=BodyLength and 10=CheckSum around the caller supplied body fields. */
void* xfix_build(cstr sBeginString, xvalue* pFields)
{
    xbuffer* pBody;
    xbuffer* pOutput;
    size_t i;
    size_t iCount;
    char aHead[64];
    char aChecksum[16];
    int iHeadLength;
    int iChecksumLength;
    bytes pResult;
    size_t iBeginLength;

    xrtClearError();
    if (sBeginString == NULL || sBeginString[0] == '\0' || pFields == NULL ||
        xrtValueType(pFields) != XVALUE_ARRAY) {
        xfix_error(XERR_ARGUMENT, "FIX build needs a BeginString and an array of field objects");
        return NULL;
    }
    if (!xfix_text_size(sBeginString, false, &iBeginLength)) return NULL;
    iCount = xrtValueCount(pFields);
    if (iCount > XFIX_MAX_FIELDS) { xfix_error(XERR_RANGE, "FIX has too many fields"); return NULL; }
    pBody = xrtBufferCreate();
    pOutput = xrtBufferCreate();
    if (pBody == NULL || pOutput == NULL) { xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL; }
    for (i = 0u; i < iCount; ++i) {
        xvalue* pField = xrtValueArrayGet(pFields, i);
        xvalue* pTag;
        xvalue* pValue;
        int64 iTag;
        xstrview Value;
        char aValue[96];
        if (pField == NULL || xrtValueType(pField) != XVALUE_OBJECT ||
            (pTag = xrtValueObjectGet(pField, XRT_STR_LITERAL("tag"))) == NULL ||
            (pValue = xrtValueObjectGet(pField, XRT_STR_LITERAL("value"))) == NULL ||
            !xrtValueGetInt(pTag, &iTag) || iTag == 8 || iTag == 9 || iTag == 10 ||
            !xfix_value_text(pValue, aValue, sizeof(aValue), &Value) || !xfix_field_text(pBody, iTag, Value)) {
            if (xrtGetError() == NULL) { xfix_error(XERR_VALUE, "FIX body fields need tag and value; tags 8, 9, 10 are generated"); }
            xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
        }
    }
    iHeadLength = snprintf(aHead, sizeof(aHead), "9=%llu" "\x01",
        (unsigned long long)pBody->Size);
    if (iHeadLength <= 0 || (size_t)iHeadLength >= sizeof(aHead) ||
        !xfix_field_text(pOutput, 8, (xstrview){ sBeginString, iBeginLength }) ||
        !xfix_append(pOutput, aHead, (size_t)iHeadLength) ||
        !xfix_append(pOutput, pBody->Data, pBody->Size)) {
        xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
    }
    iChecksumLength = snprintf(aChecksum, sizeof(aChecksum), "10=%03u\x01", xfix_checksum_bytes(pOutput->Data, pOutput->Size));
    if (iChecksumLength <= 0 || !xfix_append(pOutput, aChecksum, (size_t)iChecksumLength)) {
        xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
    }
    if (!xrtBufferAppendByte(pOutput, 0u)) {
        xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
    }
    pResult = xrtBufferTake(pOutput, NULL, NULL);
    xrtBufferDestroy(pBody);
    xrtBufferDestroy(pOutput);
    return pResult;
}

static bool xfix_parse_uint(xstrview Text, int64* pValue)
{
    size_t i;
    int64 iValue = 0;
    if (Text.Size == 0u) { return false; }
    for (i = 0u; i < Text.Size; ++i) {
        int64 iDigit = Text.Data[i] - '0';
        if (iDigit < 0 || iDigit > 9 || iValue > (INT64_MAX - iDigit) / 10) { return false; }
        iValue = iValue * 10 + iDigit;
    }
    *pValue = iValue;
    return true;
}

static bool xfix_parse_tag(xstrview Text, int64* pTag)
{
    /* TagNum 不允许前导零；BodyLength 与字段值仍按各自规则处理。 */
    return Text.Size != 0u && Text.Data[0] != '0' && xfix_parse_uint(Text, pTag) && *pTag > 0;
}

xvalue* xfix_parse(cstr sMessage)
{
    const uint8* pData = (const uint8*)sMessage;
    size_t iSize = 0u;
    size_t iOffset = 0u;
    size_t iCount = 0u;
    size_t iBodyStart = 0u;
    size_t iChecksumStart = 0u;
    int64 iBodyLength = -1;
    int64 iChecksum = -1;
    xvalue* pResult;
    xvalue* pFields;

    xrtClearError();
    if (sMessage != NULL)
        while (iSize <= XFIX_MAX_BYTES && sMessage[iSize] != '\0') ++iSize;
    if (iSize == 0u) {
        xfix_error(XERR_ARGUMENT, "FIX input must not be empty"); return xrtValueNull();
    }
    if (iSize > XFIX_MAX_BYTES) {
        xfix_error(XERR_RANGE, "FIX input exceeds the 16 MiB safety limit"); return xrtValueNull();
    }
    pResult = xrtValueObject();
    pFields = xrtValueArray();
    if (pResult == NULL || pFields == NULL) { xrtValueRelease(pResult); xrtValueRelease(pFields); return xrtValueNull(); }
    while (iOffset < iSize) {
        size_t iStart = iOffset;
        size_t iEqual = iOffset;
        size_t iEnd;
        int64 iTag;
        xvalue* pField;
        xvalue* pValue;
        while (iEqual < iSize && pData[iEqual] != '=') { ++iEqual; }
        if (iEqual == iSize) { xfix_error(XERR_VALUE, "FIX field lacks '='"); goto fail; }
        iEnd = iEqual + 1u;
        while (iEnd < iSize && pData[iEnd] != '\x01') { ++iEnd; }
        if (iEnd == iSize || !xfix_parse_tag((xstrview){ (cstr)pData + iStart, iEqual - iStart }, &iTag)) {
            xfix_error(XERR_VALUE, "invalid FIX field or missing SOH terminator"); goto fail;
        }
        /* 上限针对应用字段；8、9、10 是额外生成的三个帧字段。 */
        if (++iCount > XFIX_MAX_FIELDS + 3u) { xfix_error(XERR_RANGE, "FIX has too many fields"); goto fail; }
        if ((iCount == 1u && iTag != 8) || (iCount == 2u && iTag != 9) ||
            (iCount > 2u && (iTag == 8 || iTag == 9))) {
            xfix_error(XERR_VALUE, "FIX requires tags 8 and 9 first, without duplicates"); goto fail;
        }
        if (iTag == 8 && iEnd == iEqual + 1u) {
            xfix_error(XERR_VALUE, "FIX BeginString cannot be empty"); goto fail;
        }
        if (iTag == 9) {
            xstrview BodyText = { (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u };
            if (!xfix_parse_uint(BodyText, &iBodyLength)) { xfix_error(XERR_VALUE, "FIX BodyLength is invalid"); goto fail; }
            iBodyStart = iEnd + 1u;
        }
        if (iTag == 10) {
            xstrview ChecksumText = { (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u };
            if (ChecksumText.Size != 3u || !xfix_parse_uint(ChecksumText, &iChecksum) || iChecksum > 255) { xfix_error(XERR_VALUE, "FIX CheckSum is invalid"); goto fail; }
            iChecksumStart = iStart;
            if (iEnd + 1u != iSize) { xfix_error(XERR_VALUE, "FIX CheckSum must be the final field"); goto fail; }
        }
        pField = xrtValueObject();
        pValue = xrtValueString((xstrview){ (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u });
        if (pField == NULL || pValue == NULL || !xrtValueObjectSetNew(pField, XRT_STR_LITERAL("tag"), xrtValueInt(iTag)) ||
            !xrtValueObjectSetTake(pField, XRT_STR_LITERAL("value"), &pValue) || !xrtValueArrayAppendTake(pFields, &pField)) {
            xrtValueRelease(pField); xrtValueRelease(pValue); goto fail;
        }
        iOffset = iEnd + 1u;
    }
    if (iCount < 3u || iBodyLength < 0 || iChecksum < 0 || iChecksumStart < iBodyStart) { xfix_error(XERR_VALUE, "FIX message needs tags 8, 9 and final 10"); goto fail; }
    if ((uint64)iBodyLength != (uint64)(iChecksumStart - iBodyStart) ||
        xfix_checksum_bytes(pData, iChecksumStart) != (unsigned int)iChecksum) { xfix_error(XERR_VALUE, "FIX BodyLength or CheckSum validation failed"); goto fail; }
    if (!xrtValueObjectSetTake(pResult, XRT_STR_LITERAL("fields"), &pFields) ||
        !xrtValueObjectSetNew(pResult, XRT_STR_LITERAL("valid"), xrtValueBool(true))) { goto fail; }
    return pResult;
fail:
    xrtValueRelease(pFields);
    xrtValueRelease(pResult);
    return xrtValueNull();
}

bool xfix_validate(cstr sMessage)
{
    xvalue* pResult = xfix_parse(sMessage);
    bool bValid = pResult != NULL && xrtValueType(pResult) == XVALUE_OBJECT;
    const xerror* pError = xrtGetError();
    xrtValueRelease(pResult);
    if (!bValid && (pError == NULL || xrtErrorKind(pError) != XERR_MEMORY))
        xrtClearError();
    return bValid;
}

bool xfix_has_error(void)
{
    return xrtGetError() != NULL;
}

bool xfix_memory_error(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL && xrtErrorKind(pError) == XERR_MEMORY;
}

bool xfix_range_error(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL && xrtErrorKind(pError) == XERR_RANGE;
}

xvalue* xfix_field(int64 iTag, cstr sValue)
{
    xvalue* pField;
    xvalue* pValue;
    size_t iValueLength;
    xrtClearError();
    if (iTag <= 0 || iTag == 8 || iTag == 9 || iTag == 10 ||
        sValue == NULL) {
        xfix_error(XERR_VALUE, "FIX body field needs a positive non-header tag and no SOH");
        return xrtValueNull();
    }
    if (!xfix_text_size(sValue, true, &iValueLength)) return xrtValueNull();
    pField = xrtValueObject();
    if (pField == NULL) return xrtValueNull();
    pValue = xrtValueString((xstrview){ sValue, iValueLength });
    if (pValue == NULL || !xrtValueObjectSetNew(pField, XRT_STR_LITERAL("tag"), xrtValueInt(iTag)) ||
        !xrtValueObjectSetTake(pField, XRT_STR_LITERAL("value"), &pValue)) { xrtValueRelease(pValue); xrtValueRelease(pField); return xrtValueNull(); }
    return pField;
}

cstr xfix_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "FIX operation failed";
}
