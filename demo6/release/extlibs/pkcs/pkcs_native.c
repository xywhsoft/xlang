/*
	PKCS DER 便捷接口。
	这一层只处理 xlang 使用的 hex 文本；DER 规则、边界和编码全部复用 XRT ASN.1 模块。
*/
#include <stdio.h>
#include <string.h>

#include <limits.h>

#define XRT_MODULE_ASN1_DER
#define XRT_MODULE_BUFFER_HEX
#include "xrt.h"

#define XPKCS_DOMAIN "xlang.extlib.pkcs"
#define XPKCS_RESULT_SLOTS 8u

#if defined(_WIN32) || defined(_WIN64)
#define XPKCS_THREAD __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define XPKCS_THREAD __thread
#else
#define XPKCS_THREAD
#endif

static XPKCS_THREAD xbuffer g_xpkcs_results[XPKCS_RESULT_SLOTS];
static XPKCS_THREAD size_t g_xpkcs_result_index;



static void xpkcs_error(xerrkind Kind, cstr sMessage)
{
    xrtSetErrorInfo(Kind, XPKCS_DOMAIN, 0, sMessage);
}



/* 不引入 XRT string 模块；封装层只需要构造借用文本视图。 */
static xstrview xpkcs_view(cstr sText)
{
    return (xstrview){ sText, sText != NULL ? strlen(sText) : 0u };
}



/* 遵循源码型扩展库的统一失败查询约定。 */
cstr xpkcs_last_error(void)
{
    const xerror* pError = xrtGetError();
    cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

    return sMessage != NULL ? sMessage : "";
}



/* 把临时 DER 缓冲安全转交线程局部结果槽。 */
static cstr xpkcs_result_hex(const xbuffer* pDer)
{
    str sHex;

    if (pDer == NULL) {
        xpkcs_error(XERR_ARGUMENT, "DER result buffer is invalid");
        return "";
    }
    sHex = xrtHexEncodeNew(pDer->Data, pDer->Size, 0u);
    if (sHex == NULL) {
        xpkcs_error(XERR_MEMORY, "could not encode DER result as hexadecimal text");
        return "";
    }
    {
        size_t iSlot = g_xpkcs_result_index++ % XPKCS_RESULT_SLOTS;
        xbuffer* pSlot = &g_xpkcs_results[iSlot];

        xrtBufferUnit(pSlot);
        if (!xrtBufferAppend(pSlot, (xbytesview){ (cbytes)sHex, strlen(sHex) }) ||
            !xrtBufferAppendByte(pSlot, 0)) {
            xrtFree(sHex);
            xpkcs_error(XERR_MEMORY, "could not store DER hexadecimal result");
            return "";
        }
        xrtFree(sHex);
        xrtClearError();
        return (cstr)pSlot->Data;
    }
}



/* 把一段 UTF-8/ASCII 文本写入线程局部结果槽。 */
static cstr xpkcs_result_text(xbytesview Text)
{
    size_t iSlot = g_xpkcs_result_index++ % XPKCS_RESULT_SLOTS;
    xbuffer* pSlot = &g_xpkcs_results[iSlot];

    xrtBufferUnit(pSlot);
    if (!xrtBufferAppend(pSlot, Text) || !xrtBufferAppendByte(pSlot, 0)) {
        xpkcs_error(XERR_MEMORY, "could not store text result");
        return "";
    }
    xrtClearError();
    return (cstr)pSlot->Data;
}



/* 解码严格 hex 输入，并视需要求为只含一个完整 DER 值。 */
static xbuffer* xpkcs_parse(cstr sHex, xdervalue* pValue, bool bWhole)
{
    xbuffer* pDer;
    xdercursor Cursor;

    if ((sHex == NULL) || (pValue == NULL)) {
        xpkcs_error(XERR_ARGUMENT, "DER hexadecimal text is invalid");
        return NULL;
    }
    pDer = xrtBufferFromHex(xpkcs_view(sHex), 0u);
    if ((pDer == NULL) || (pDer->Size == 0u) ||
        (bWhole && !xrtDerValidate(pDer->Data, pDer->Size)) ||
        !xrtDerInit(&Cursor, pDer->Data, pDer->Size) ||
        (xrtDerRead(&Cursor, pValue) != XDER_VALUE) ||
        (bWhole && !xrtDerDone(&Cursor))) {
        xrtBufferDestroy(pDer);
        xpkcs_error(XERR_PROTOCOL, "input must contain exactly one valid DER value");
        return NULL;
    }
    return pDer;
}



/* 解码 hex 内容，空串在封装类 API 中合法。 */
static xbuffer* xpkcs_content(cstr sHex)
{
    if (sHex == NULL) {
        xpkcs_error(XERR_ARGUMENT, "hexadecimal text is invalid");
        return NULL;
    }
    return xrtBufferFromHex(xpkcs_view(sHex), 0u);
}



long long xpkcs_get_tag(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    long long iResult;

    pDer = xpkcs_parse(sHex, &Value, true);
    if (pDer == NULL) { return -1; }
    /* 保持 xlang 已有 API 口径：getTag 返回 DER 首个标签字节。 */
    iResult = (long long)Value.Raw.Data[0];
    xrtBufferDestroy(pDer);
    xrtClearError();
    return iResult;
}



long long xpkcs_get_length(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    long long iResult;

    pDer = xpkcs_parse(sHex, &Value, true);
    if (pDer == NULL) { return -1; }
    if (Value.Value.Size > (size_t)INT64_MAX) {
        xrtBufferDestroy(pDer);
        xpkcs_error(XERR_RANGE, "DER content length exceeds xlang int range");
        return -1;
    }
    iResult = (long long)Value.Value.Size;
    xrtBufferDestroy(pDer);
    xrtClearError();
    return iResult;
}



cstr xpkcs_get_content(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    xbuffer Content;
    cstr sResult;

    pDer = xpkcs_parse(sHex, &Value, true);
    if (pDer == NULL) { return ""; }
    if (!xrtBufferInit(&Content) || !xrtBufferAppend(&Content, Value.Value)) {
        xrtBufferDestroy(pDer);
        xrtBufferUnit(&Content);
        return "";
    }
    sResult = xpkcs_result_hex(&Content);
    xrtBufferUnit(&Content);
    xrtBufferDestroy(pDer);
    return sResult;
}



cstr xpkcs_encode_int(long long iValue)
{
    xbuffer Der;
    cstr sResult;

    if (!xrtBufferInit(&Der) || !xrtDerAppendInt64(&Der, (int64)iValue)) {
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    return sResult;
}



long long xpkcs_decode_int(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    int64 iResult;

    pDer = xpkcs_parse(sHex, &Value, true);
    if ((pDer == NULL) || !xrtDerInt64(&Value, &iResult)) {
        xrtBufferDestroy(pDer);
        return 0;
    }
    xrtBufferDestroy(pDer);
    xrtClearError();
    return (long long)iResult;
}



cstr xpkcs_encode_oid(cstr sDotted)
{
    xbuffer Der;
    cstr sResult;

    if ((sDotted == NULL) || !xrtBufferInit(&Der) ||
        !xrtDerAppendOid(&Der, xpkcs_view(sDotted))) {
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    return sResult;
}



cstr xpkcs_decode_oid(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    xbytesview Oid;
    xbuffer Text;
    cstr sResult;

    pDer = xpkcs_parse(sHex, &Value, true);
    if (pDer == NULL) { return ""; }
    if (!xrtDerOid(&Value, &Oid) || !xrtBufferInit(&Text) ||
        !xrtDerOidDecode(Oid, &Text) || !xrtBufferAppendByte(&Text, 0)) {
        xrtBufferDestroy(pDer);
        xrtBufferUnit(&Text);
        return "";
    }
    sResult = xpkcs_result_text((xbytesview){ Text.Data, Text.Size - 1u });
    xrtBufferUnit(&Text);
    xrtBufferDestroy(pDer);
    return sResult;
}



static cstr xpkcs_wrap(cstr sHex, uint32 iTag)
{
    xbuffer* pContent;
    xbuffer Der;
    cstr sResult;

    pContent = xpkcs_content(sHex);
    if ((pContent == NULL) || !xrtBufferInit(&Der) ||
        !xrtDerAppend(&Der, XASN1_UNIVERSAL, iTag, true, xrtBufferView(pContent))) {
        xrtBufferDestroy(pContent);
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    xrtBufferDestroy(pContent);
    return sResult;
}



cstr xpkcs_wrap_sequence(cstr sHex)
{
    return xpkcs_wrap(sHex, (uint32)XASN1_SEQUENCE);
}



cstr xpkcs_wrap_set(cstr sHex)
{
    return xpkcs_wrap(sHex, (uint32)XASN1_SET);
}



cstr xpkcs_encode_octet(cstr sHex)
{
    xbuffer* pContent;
    xbuffer Der;
    cstr sResult;

    pContent = xpkcs_content(sHex);
    if ((pContent == NULL) || !xrtBufferInit(&Der) ||
        !xrtDerAppendOctets(&Der, xrtBufferView(pContent))) {
        xrtBufferDestroy(pContent);
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    xrtBufferDestroy(pContent);
    return sResult;
}



cstr xpkcs_encode_null(void)
{
    xbuffer Der;
    cstr sResult;

    if (!xrtBufferInit(&Der) || !xrtDerAppendNull(&Der)) {
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    return sResult;
}



cstr xpkcs_encode_bitstring(cstr sHex)
{
    xbuffer* pContent;
    xbuffer Der;
    cstr sResult;

    pContent = xpkcs_content(sHex);
    if ((pContent == NULL) || !xrtBufferInit(&Der) ||
        !xrtDerAppendBitString(&Der, xrtBufferView(pContent), 0)) {
        xrtBufferDestroy(pContent);
        xrtBufferUnit(&Der);
        return "";
    }
    sResult = xpkcs_result_hex(&Der);
    xrtBufferUnit(&Der);
    xrtBufferDestroy(pContent);
    return sResult;
}



cstr xpkcs_concat(cstr sLeft, cstr sRight)
{
    xbuffer Text;
    cstr sResult;

    if ((sLeft == NULL) || (sRight == NULL)) {
        xpkcs_error(XERR_ARGUMENT, "hexadecimal text is invalid");
        return "";
    }
    if (!xrtBufferInit(&Text) ||
        !xrtBufferAppend(&Text, (xbytesview){ (cbytes)sLeft, strlen(sLeft) }) ||
        !xrtBufferAppend(&Text, (xbytesview){ (cbytes)sRight, strlen(sRight) })) {
        xrtBufferUnit(&Text);
        return "";
    }
    sResult = xpkcs_result_text(xrtBufferView(&Text));
    xrtBufferUnit(&Text);
    return sResult;
}



long long xpkcs_der_len(cstr sHex)
{
    xbuffer* pDer;
    long long iResult;

    if ((sHex == NULL) || !(pDer = xrtBufferFromHex(xpkcs_view(sHex), 0u))) {
        return -1;
    }
    if (pDer->Size > (size_t)INT64_MAX) {
        xrtBufferDestroy(pDer);
        xpkcs_error(XERR_RANGE, "DER byte length exceeds xlang int range");
        return -1;
    }
    iResult = (long long)pDer->Size;
    xrtBufferDestroy(pDer);
    xrtClearError();
    return iResult;
}



long long xpkcs_next_offset(cstr sHex, long long iOffset)
{
    xbuffer* pDer;
    xdercursor Cursor;
    xdervalue Value;
    size_t iResult;

    pDer = xpkcs_content(sHex);
    if ((iOffset < 0) || (pDer == NULL) || ((uint64)iOffset >= pDer->Size) ||
        !xrtDerInit(&Cursor, pDer->Data + (size_t)iOffset,
            pDer->Size - (size_t)iOffset) ||
        (xrtDerRead(&Cursor, &Value) != XDER_VALUE)) {
        xrtBufferDestroy(pDer);
        xpkcs_error(XERR_PROTOCOL, "DER offset does not point to a complete value");
        return -1;
    }
    iResult = (size_t)iOffset + Value.Raw.Size;
    xrtBufferDestroy(pDer);
    xrtClearError();
    return (long long)iResult;
}



cstr xpkcs_tag_name(cstr sHex)
{
    xbuffer* pDer;
    xdervalue Value;
    cstr sResult;
    char Text[48];

    pDer = xpkcs_parse(sHex, &Value, true);
    if (pDer == NULL) { return ""; }
    if ((Value.Tag.Class == XASN1_UNIVERSAL) && !Value.Tag.Constructed) {
        switch (Value.Tag.Number) {
            case XASN1_INTEGER: sResult = "INTEGER"; break;
            case XASN1_BIT_STRING: sResult = "BIT STRING"; break;
            case XASN1_OCTET_STRING: sResult = "OCTET STRING"; break;
            case XASN1_NULL: sResult = "NULL"; break;
            case XASN1_OBJECT_IDENTIFIER: sResult = "OID"; break;
            default: sResult = NULL; break;
        }
    } else if ((Value.Tag.Class == XASN1_UNIVERSAL) && Value.Tag.Constructed &&
        (Value.Tag.Number == XASN1_SEQUENCE)) {
        sResult = "SEQUENCE";
    } else if ((Value.Tag.Class == XASN1_UNIVERSAL) && Value.Tag.Constructed &&
        (Value.Tag.Number == XASN1_SET)) {
        sResult = "SET";
    } else {
        sResult = NULL;
    }
    if (sResult == NULL) {
        (void)snprintf(Text, sizeof(Text), "TAG(%u)", (unsigned int)Value.Tag.Number);
        sResult = Text;
    }
    sResult = xpkcs_result_text((xbytesview){ (cbytes)sResult, strlen(sResult) });
    xrtBufferDestroy(pDer);
    return sResult;
}
