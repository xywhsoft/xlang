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
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.xml.etree",
			XRT_XML_ERROR_FORMAT, "XML ElementTree operation failed");
		return;
	}
	xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.xml.etree", pError->iCode,
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

XRT_API ptr xetree_parse(ptr pData, uint64 iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xetreeClearError(pError);
	pResult = xrtXmlElementParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

XRT_API ptr xetree_load(str sPath, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	__xetreeOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xetreeClearError(pError);
	pResult = xrtXmlElementLoad(sPath, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

XRT_API ptr xetree_create(str sName) { return xrtXmlElementCreate(sName); }
XRT_API ptr xetree_adopt(ptr pHandle) { return pHandle; }
XRT_API void xetree_release(ptr pHandle) { xrtXmlElementRelease((xrtxmlelement*)pHandle); }
XRT_API int64 xetree_kind(ptr pHandle) { return xrtXmlElementKind((xrtxmlelement*)pHandle); }
XRT_API str xetree_name(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementName((xrtxmlelement*)pHandle)); }
XRT_API str xetree_local_name(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementLocalName((xrtxmlelement*)pHandle)); }
XRT_API str xetree_namespace_uri(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementNamespaceUri((xrtxmlelement*)pHandle)); }
XRT_API str xetree_prefix(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementPrefix((xrtxmlelement*)pHandle)); }
XRT_API str xetree_text(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementText((xrtxmlelement*)pHandle)); }
XRT_API str xetree_tail(ptr pHandle) { return __xrtXmlEtreeCopy((const char*)xrtXmlElementTail((xrtxmlelement*)pHandle)); }
XRT_API bool xetree_set_text(ptr pHandle, str sText) { return xrtXmlElementSetText((xrtxmlelement*)pHandle, sText); }
XRT_API bool xetree_set_tail(ptr pHandle, str sText) { return xrtXmlElementSetTail((xrtxmlelement*)pHandle, sText); }
XRT_API int64 xetree_child_count(ptr pHandle) { return xrtXmlElementChildCount((xrtxmlelement*)pHandle); }
XRT_API ptr xetree_child(ptr pHandle, uint32 iIndex) { return xrtXmlElementChild((xrtxmlelement*)pHandle, iIndex); }
XRT_API ptr xetree_append(ptr pHandle, str sName) { return xrtXmlElementAppend((xrtxmlelement*)pHandle, sName); }
XRT_API str xetree_get_attribute(ptr pHandle, str sName) {
	str sValue = xrtXmlElementGetAttribute((xrtxmlelement*)pHandle, sName);
	return sValue != NULL ? __xrtXmlEtreeCopy((const char*)sValue) : NULL;
}
XRT_API bool xetree_set_attribute(ptr pHandle, str sName, str sValue) { return xrtXmlElementSetAttribute((xrtxmlelement*)pHandle, sName, sValue); }
XRT_API bool xetree_remove_attribute(ptr pHandle, str sName) { return xrtXmlElementRemoveAttribute((xrtxmlelement*)pHandle, sName); }
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
XRT_API ptr xetree_find(ptr pHandle, str sPath) { return xrtXmlElementFind((xrtxmlelement*)pHandle, sPath); }
XRT_API xvalue* xetree_find_all(ptr pHandle, str sPath) { return xrtXmlElementFindAll((xrtxmlelement*)pHandle, sPath); }

static void __xetreeWriteOptions(xrtxmletreewriteoptions* pOptions,
	bool bPretty, uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtXmlElementWriteOptionsInit(pOptions);
	pOptions->bPretty = bPretty;
	pOptions->iIndent = iIndent;
	pOptions->bXmlDeclaration = bDeclaration;
	pOptions->iMaxOutputBytes = iMaxOutput;
}

XRT_API ptr xetree_stringify(ptr pHandle, bool bPretty, uint32 iIndent,
	bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	ptr pResult;
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xetreeClearError(pError);
	pResult = xrtXmlElementStringify((xrtxmlelement*)pHandle, &tOptions, pError);
	if ( pResult == NULL ) { __xetreePublishError(pError); }
	return pResult;
}

XRT_API bool xetree_save(ptr pHandle, str sPath, bool bPretty,
	uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtxmletreewriteoptions tOptions;
	xrtxmlerror* pError = __xetreeError();
	bool bResult;
	__xetreeWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xetreeClearError(pError);
	bResult = xrtXmlElementSave(sPath, (xrtxmlelement*)pHandle, &tOptions, pError);
	if ( !bResult ) { __xetreePublishError(pError); }
	return bResult;
}

XRT_API int64 xetree_error_code(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? p->iCode : XRT_XML_ERROR_MEMORY; }
XRT_API int64 xetree_error_line(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iLine : 0; }
XRT_API int64 xetree_error_column(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iColumn : 0; }
XRT_API int64 xetree_error_offset(void) { xrtxmlerror* p = __xetreeError(); return p != NULL ? (int64)p->iOffset : 0; }
XRT_API str xetree_error_message(void) { xrtxmlerror* p = __xetreeError(); return __xrtXmlEtreeCopy(p != NULL ? p->sMessage : "XML ElementTree state is unavailable"); }
