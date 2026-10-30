#include "xrt_xml_etree_internal.h"

#include <stdio.h>
#include <string.h>



static xonce __xetreeErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xetreeErrorKey = NULL;

static bool __xetreeInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xetreeErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xetreeErrorKey != NULL;
}

/* ElementTree 的错误状态与 DOM/SAX 一样按调用线程隔离。 */
static xrtxmlerror* __xetreeError(void)
{
	xrtxmlerror* pError;

	if ( !xrtOnce(&__xetreeErrorOnce, __xetreeInitErrorKey, NULL) ||
		__xetreeErrorKey == NULL ) {
		return NULL;
	}
	pError = (xrtxmlerror*)xrtThreadKeyGet(__xetreeErrorKey);
	if ( pError != NULL ) { return pError; }
	pError = (xrtxmlerror*)xrtMalloc(sizeof(*pError));
	if ( pError == NULL || !xrtThreadKeySet(__xetreeErrorKey, pError) ) {
		xrtFree(pError);
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return pError;
}

static void __xetreeClearError(xrtxmlerror* pError)
{
	xrtClearError();
	if ( pError != NULL ) { xrtXmlErrorInit(pError); }
}

static void __xetreePublishError(const xrtxmlerror* pError)
{
	if ( pError == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.xml.etree",
			XRT_XML_ERROR_MEMORY, "XML ElementTree error state is unavailable");
		return;
	}
	xrtSetErrorInfo(pError->iCode == XRT_XML_ERROR_MEMORY ? XERR_MEMORY : XERR_VALUE,
		"xlang.extlib.xml.etree", pError->iCode,
		pError->sMessage[0] != 0 ? pError->sMessage : "XML ElementTree operation failed");
}

static void __xetreeOptions(xrtxmloptions* pOptions, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtXmlOptionsInit(pOptions);
	pOptions->tLimits.iMaxInputBytes = iMaxInput;
	pOptions->tLimits.iMaxOutputBytes = iMaxOutput;
	pOptions->tLimits.iMaxItemBytes = iMaxItem;
	pOptions->tLimits.iMaxEntries = iMaxEntries;
	pOptions->tLimits.iMaxNodes = iMaxNodes;
	pOptions->tLimits.iMaxDepth = iMaxDepth;
}

XRT_API void* xetree_parse(const char* pData, unsigned long long iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, unsigned int iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	/* 错误槽创建失败时保留原错误，不能清错后继续解析。 */
	if ( pError == NULL ) { return NULL; }
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xetreeClearError(pError);
	pResult = xrtXmlElementParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

XRT_API void* xetree_load(const char* sPath, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned long long iMaxNodes,
	unsigned int iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	if ( pError == NULL ) { return NULL; }
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xetreeClearError(pError);
	pResult = xrtXmlElementLoad((str)sPath, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

XRT_API void* xetree_create(const char* sName) { return xrtXmlElementCreate((str)sName); }
XRT_API void* xetree_adopt(void* pHandle) { return pHandle; }
XRT_API void xetree_release(void* pHandle) { xrtXmlElementRelease((xrtxmlelement*)pHandle); }
XRT_API int64 xetree_kind(ptr pHandle) { return xrtXmlElementKind((xrtxmlelement*)pHandle); }
XRT_API void* xetree_name(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementName((xrtxmlelement*)pHandle)); }
XRT_API void* xetree_local_name(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementLocalName((xrtxmlelement*)pHandle)); }
XRT_API void* xetree_namespace_uri(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementNamespaceUri((xrtxmlelement*)pHandle)); }
XRT_API void* xetree_prefix(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementPrefix((xrtxmlelement*)pHandle)); }
XRT_API void* xetree_text(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementText((xrtxmlelement*)pHandle)); }
XRT_API void* xetree_tail(void* pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementTail((xrtxmlelement*)pHandle)); }
XRT_API bool xetree_set_text(void* pHandle, const char* sText) { return xrtXmlElementSetText((xrtxmlelement*)pHandle, (str)sText); }
XRT_API bool xetree_set_tail(void* pHandle, const char* sText) { return xrtXmlElementSetTail((xrtxmlelement*)pHandle, (str)sText); }
XRT_API int64 xetree_child_count(ptr pHandle) { return xrtXmlElementChildCount((xrtxmlelement*)pHandle); }
XRT_API ptr xetree_child(ptr pHandle, uint32 iIndex) { return xrtXmlElementChild((xrtxmlelement*)pHandle, iIndex); }
XRT_API void* xetree_append(void* pHandle, const char* sName) { return xrtXmlElementAppend((xrtxmlelement*)pHandle, (str)sName); }
XRT_API void* xetree_get_attribute(void* pHandle, const char* sName) {
	str sValue = xrtXmlElementGetAttribute((xrtxmlelement*)pHandle, (str)sName);
	return sValue != NULL ? __xrtXmlEtreeCopy((const char*)sValue) : NULL;
}
XRT_API bool xetree_set_attribute(void* pHandle, const char* sName, const char* sValue) { return xrtXmlElementSetAttribute((xrtxmlelement*)pHandle, (str)sName, (str)sValue); }
XRT_API bool xetree_remove_attribute(void* pHandle, const char* sName) { return xrtXmlElementRemoveAttribute((xrtxmlelement*)pHandle, (str)sName); }
XRT_API xvalue* xetree_attributes(ptr pHandle) {
	xrtxmlelement* pElement = (xrtxmlelement*)pHandle;
	xvalue* pResult = xrtValueObject();
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	for ( i = 0u; i < xrtXmlElementAttributeCount(pElement); i++ ) {
		str sName = xrtXmlElementAttributeName(pElement, i);
		str sValue = xrtXmlElementAttributeValue(pElement, i);
		xvalue* pValue = xrtValueString(xrtStrView(sValue != NULL ? sValue : ""));
		if ( sName == NULL || pValue == NULL || !xrtValueObjectSetTake(pResult,
			xrtStrView(sName), &pValue) ) {
			xrtValueRelease(pValue);
			xrtValueRelease(pResult);
			return NULL;
		}
	}
	return pResult;
}
XRT_API void* xetree_find(void* pHandle, const char* sPath) { return xrtXmlElementFind((xrtxmlelement*)pHandle, (str)sPath); }
XRT_API xvalue* xetree_find_all(void* pHandle, const char* sPath) { return xrtXmlElementFindAll((xrtxmlelement*)pHandle, (str)sPath); }

static void __xetreeWriteOptions(xrtxmletreewriteoptions* pOptions,
	bool bPretty, uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtXmlElementWriteOptionsInit(pOptions);
	pOptions->bPretty = bPretty;
	pOptions->iIndent = iIndent;
	pOptions->bXmlDeclaration = bDeclaration;
	pOptions->iMaxOutputBytes = iMaxOutput;
}

XRT_API void* xetree_stringify(void* pHandle, bool bPretty, unsigned int iIndent,
	bool bDeclaration, unsigned long long iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	if ( pError == NULL ) { return NULL; }
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xetreeClearError(pError);
	pResult = xrtXmlElementStringify((xrtxmlelement*)pHandle, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

/* 文本缓冲所有权转移给调用方，由 xrtFree 释放。 */
XRT_API void* xetree_stringify_text(void* pHandle, bool bPretty,
	unsigned int iIndent, bool bDeclaration, unsigned long long iMaxOutput)
{
	xbuffer* pResult = (xbuffer*)xetree_stringify(pHandle, bPretty,
		iIndent, bDeclaration, iMaxOutput);
	char* sText;
	if ( pResult == NULL ) { return NULL; }
	if ( !xrtBufferAppendByte(pResult, 0) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	sText = (char*)xrtBufferTake(pResult, NULL, NULL);
	xrtBufferDestroy(pResult);
	return sText;
}

XRT_API bool xetree_save(void* pHandle, const char* sPath, bool bPretty,
	unsigned int iIndent, bool bDeclaration, unsigned long long iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	bool bResult;
	if ( pError == NULL ) { return false; }
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xetreeClearError(pError);
	bResult = xrtXmlElementSave((str)sPath, (xrtxmlelement*)pHandle, &tOptions, pError);
	if ( !bResult ) { __xetreePublishError(pError); }
	return bResult;
}

XRT_API int64 xetree_error_code(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? p->iCode : XRT_XML_ERROR_MEMORY; }
XRT_API int64 xetree_error_line(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iLine : 0; }
XRT_API int64 xetree_error_column(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iColumn : 0; }
XRT_API int64 xetree_error_offset(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iOffset : 0; }
XRT_API void* xetree_error_message(void) { xrtxmlerror* p = __xetreeError(); return __xrtXmlEtreeCopy(p != NULL ? p->sMessage : "XML ElementTree state is unavailable"); }
