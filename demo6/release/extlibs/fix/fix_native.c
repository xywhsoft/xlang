/* FIX 4.x message codec. Session, reconnect and venue dictionaries stay outside this package. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XFIX_DOMAIN "xlang.extlib.fix"
#define XFIX_MAX_FIELDS 4096u
#define XFIX_MAX_BYTES (16u * 1024u * 1024u)
#define XFIX_RESULT_SLOTS 8u

#if defined(_WIN32) || defined(_WIN64)
#define XFIX_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XFIX_THREAD __thread
#else
#define XFIX_THREAD
#endif

static XFIX_THREAD xbuffer g_xfix_results[XFIX_RESULT_SLOTS];
static XFIX_THREAD size_t g_xfix_result_index;

static void xfix_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XFIX_DOMAIN, 0, sMessage);
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
    if (iTag <= 0 || Value.Size > XFIX_MAX_BYTES) {
        xfix_error(XERR_VALUE, "FIX fields need a positive tag and a bounded value");
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
    double fFloat;
    bool bBool;
    if (pValue == NULL) { return false; }
    if (xrtValueGetString(pValue, pText)) { return true; }
    if (xrtValueGetInt(pValue, &iInt)) {
        int iLength = snprintf(aScratch, iCapacity, "%lld", (long long)iInt);
        if (iLength < 0 || (size_t)iLength >= iCapacity) { return false; }
        *pText = (xstrview){ aScratch, (size_t)iLength };
        return true;
    }
    if (xrtValueGetFloat(pValue, &fFloat)) {
        int iLength = snprintf(aScratch, iCapacity, "%.17g", fFloat);
        if (iLength < 0 || (size_t)iLength >= iCapacity) { return false; }
        *pText = (xstrview){ aScratch, (size_t)iLength };
        return true;
    }
    if (xrtValueGetBool(pValue, &bBool)) {
        *pText = bBool ? XRT_STR_LITERAL("Y") : XRT_STR_LITERAL("N");
        return true;
    }
    xfix_error(XERR_VALUE, "FIX field values must be string, int, float or bool");
    return false;
}

static unsigned int xfix_checksum_bytes(const uint8* pData, size_t iSize)
{
    size_t i;
    unsigned int iSum = 0u;
    for (i = 0u; i < iSize; ++i) { iSum = (iSum + pData[i]) & 0xffu; }
    return iSum;
}

static cstr xfix_return_text(xbuffer* pText)
{
    size_t iSlot = g_xfix_result_index++ % XFIX_RESULT_SLOTS;
    xbuffer* pSlot = &g_xfix_results[iSlot];
    xrtBufferUnit(pSlot);
    if (!xrtBufferAppend(pSlot, xrtBufferView(pText)) || !xrtBufferAppendByte(pSlot, 0u)) {
        xrtBufferUnit(pSlot);
        return "";
    }
    return (cstr)pSlot->Data;
}

/* Builds 8=BeginString, 9=BodyLength and 10=CheckSum around the caller supplied body fields. */
cstr xfix_build(cstr sBeginString, xvalue* pFields)
{
    xbuffer* pBody;
    xbuffer* pOutput;
    size_t i;
    size_t iCount;
    char aHead[96];
    char aChecksum[16];
    int iHeadLength;
    int iChecksumLength;
    cstr sResult;

    if (sBeginString == NULL || sBeginString[0] == '\0' || pFields == NULL ||
        xrtValueType(pFields) != XVALUE_ARRAY) {
        xfix_error(XERR_ARGUMENT, "FIX build needs a BeginString and an array of field objects");
        return NULL;
    }
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
    iHeadLength = snprintf(aHead, sizeof(aHead), "8=%s" "\x01" "9=%llu" "\x01",
        sBeginString, (unsigned long long)pBody->Size);
    if (iHeadLength <= 0 || (size_t)iHeadLength >= sizeof(aHead) ||
        !xfix_append(pOutput, aHead, (size_t)iHeadLength) || !xfix_append(pOutput, pBody->Data, pBody->Size)) {
        xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
    }
    iChecksumLength = snprintf(aChecksum, sizeof(aChecksum), "10=%03u\x01", xfix_checksum_bytes(pOutput->Data, pOutput->Size));
    if (iChecksumLength <= 0 || !xfix_append(pOutput, aChecksum, (size_t)iChecksumLength)) {
        xrtBufferDestroy(pBody); xrtBufferDestroy(pOutput); return NULL;
    }
    sResult = xfix_return_text(pOutput);
    xrtBufferDestroy(pBody);
    xrtBufferDestroy(pOutput);
    xrtClearError();
    return sResult;
}

static bool xfix_parse_tag(xstrview Text, int64* pTag)
{
    size_t i;
    int64 iValue = 0;
    if (Text.Size == 0u) { return false; }
    for (i = 0u; i < Text.Size; ++i) {
        if (Text.Data[i] < '0' || Text.Data[i] > '9' || iValue > (INT64_MAX - 9) / 10) { return false; }
        iValue = iValue * 10 + (Text.Data[i] - '0');
    }
    *pTag = iValue;
    return iValue > 0;
}

xvalue* xfix_parse(cstr sMessage)
{
    const uint8* pData = (const uint8*)sMessage;
    size_t iSize;
    size_t iOffset = 0u;
    size_t iCount = 0u;
    size_t iBodyStart = 0u;
    size_t iChecksumStart = 0u;
    int64 iBodyLength = -1;
    int64 iChecksum = -1;
    xvalue* pResult;
    xvalue* pFields;

    if (sMessage == NULL || (iSize = strlen(sMessage)) == 0u || iSize > XFIX_MAX_BYTES) {
        xfix_error(XERR_ARGUMENT, "FIX input must contain at most 16 MiB of text"); return xrtValueNull();
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
        if (iEqual == iSize) { xfix_error(XERR_PROTOCOL, "FIX field lacks '='"); goto fail; }
        iEnd = iEqual + 1u;
        while (iEnd < iSize && pData[iEnd] != '\x01') { ++iEnd; }
        if (iEnd == iSize || !xfix_parse_tag((xstrview){ (cstr)pData + iStart, iEqual - iStart }, &iTag)) {
            xfix_error(XERR_PROTOCOL, "invalid FIX field or missing SOH terminator"); goto fail;
        }
        if (++iCount > XFIX_MAX_FIELDS) { xfix_error(XERR_RANGE, "FIX has too many fields"); goto fail; }
        if (iTag == 9) {
            xstrview BodyText = { (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u };
            if (!xfix_parse_tag(BodyText, &iBodyLength)) { xfix_error(XERR_PROTOCOL, "FIX BodyLength is invalid"); goto fail; }
            iBodyStart = iEnd + 1u;
        }
        if (iTag == 10) {
            xstrview ChecksumText = { (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u };
            if (ChecksumText.Size != 3u || !xfix_parse_tag(ChecksumText, &iChecksum) || iChecksum > 255) { xfix_error(XERR_PROTOCOL, "FIX CheckSum is invalid"); goto fail; }
            iChecksumStart = iStart;
            if (iEnd + 1u != iSize) { xfix_error(XERR_PROTOCOL, "FIX CheckSum must be the final field"); goto fail; }
        }
        pField = xrtValueObject();
        pValue = xrtValueString((xstrview){ (cstr)pData + iEqual + 1u, iEnd - iEqual - 1u });
        if (pField == NULL || pValue == NULL || !xrtValueObjectSetNew(pField, XRT_STR_LITERAL("tag"), xrtValueInt(iTag)) ||
            !xrtValueObjectSetTake(pField, XRT_STR_LITERAL("value"), &pValue) || !xrtValueArrayAppendTake(pFields, &pField)) {
            xrtValueRelease(pField); xrtValueRelease(pValue); goto fail;
        }
        iOffset = iEnd + 1u;
    }
    if (iCount < 3u || iBodyLength < 0 || iChecksum < 0 || iChecksumStart <= iBodyStart) { xfix_error(XERR_PROTOCOL, "FIX message needs tags 8, 9 and final 10"); goto fail; }
    if ((uint64)iBodyLength != (uint64)(iChecksumStart - iBodyStart) ||
        xfix_checksum_bytes(pData, iChecksumStart) != (unsigned int)iChecksum) { xfix_error(XERR_PROTOCOL, "FIX BodyLength or CheckSum validation failed"); goto fail; }
    if (!xrtValueObjectSetTake(pResult, XRT_STR_LITERAL("fields"), &pFields) ||
        !xrtValueObjectSetNew(pResult, XRT_STR_LITERAL("valid"), xrtValueBool(true))) { goto fail; }
    xrtClearError();
    return pResult;
fail:
    xrtValueRelease(pFields);
    xrtValueRelease(pResult);
    return xrtValueNull();
}

bool xfix_validate(cstr sMessage)
{
    xvalue* pResult = xfix_parse(sMessage);
    if (pResult == NULL) { return false; }
    xrtValueRelease(pResult);
    return true;
}

xvalue* xfix_field(int64 iTag, cstr sValue)
{
    xvalue* pField = xrtValueObject();
    xvalue* pValue;
    if (iTag <= 0 || sValue == NULL || pField == NULL) { xrtValueRelease(pField); xfix_error(XERR_ARGUMENT, "FIX field needs a positive tag and value"); return xrtValueNull(); }
    pValue = xrtValueString(xrtStrView(sValue));
    if (pValue == NULL || !xrtValueObjectSetNew(pField, XRT_STR_LITERAL("tag"), xrtValueInt(iTag)) ||
        !xrtValueObjectSetTake(pField, XRT_STR_LITERAL("value"), &pValue)) { xrtValueRelease(pValue); xrtValueRelease(pField); return xrtValueNull(); }
    return pField;
}

cstr xfix_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "FIX operation failed";
}
