#include "xrt_xml_dom_internal.h"

#include <string.h>



static xonce __xdomErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xdomErrorKey = NULL;

static bool __xdomInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xdomErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xdomErrorKey != NULL;
}

/* 每个调用线程保留最近一次 XML 结构化错误，供 xlang 的 error* API 查询。 */
static xrtxmlerror* __xdomError(void)
{
	xrtxmlerror* pError;

	if ( !xrtOnce(&__xdomErrorOnce, __xdomInitErrorKey, NULL) ||
		__xdomErrorKey == NULL ) {
		return NULL;
	}
	pError = (xrtxmlerror*)xrtThreadKeyGet(__xdomErrorKey);
	if ( pError != NULL ) { return pError; }
	pError = (xrtxmlerror*)xrtMalloc(sizeof(*pError));
	if ( pError == NULL || !xrtThreadKeySet(__xdomErrorKey, pError) ) {
		xrtFree(pError);
		return NULL;
	}
	xrtXmlErrorInit(pError);
	return pError;
}

static void __xdomClearError(xrtxmlerror* pError)
{
	xrtClearError();
	if ( pError != NULL ) { xrtXmlErrorInit(pError); }
}

static void __xdomPublishError(const xrtxmlerror* pError)
{
	if ( pError == NULL ) {
		xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.xml.dom",
			XRT_XML_ERROR_MEMORY, "XML DOM error state is unavailable");
		return;
	}
	xrtSetErrorInfo(pError->iCode == XRT_XML_ERROR_MEMORY ? XERR_MEMORY : XERR_VALUE,
		"xlang.extlib.xml.dom", pError->iCode,
		pError->sMessage[0] != 0 ? pError->sMessage : "XML DOM operation failed");
}

static void __xdomOptions(xrtxmloptions* pOptions, uint64 iMaxInput,
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

static str __xdomCopy(const char* sText)
{
	return __xrtXmlDomCopy(sText != NULL ? sText : "");
}

XRT_API ptr xdom_parse(const char* pData, unsigned long long iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xdomError();
	ptr pResult;
	__xdomOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xdomClearError(pError);
	pResult = xrtXmlDomParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { __xdomPublishError(pError); }
	return pResult;
}

XRT_API ptr xdom_load(const char* sPath, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned long long iMaxNodes,
	uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror* pError = __xdomError();
	ptr pResult;
	__xdomOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	__xdomClearError(pError);
	pResult = xrtXmlDomLoad((str)sPath, &tOptions, pError);
	if ( pResult == NULL ) { __xdomPublishError(pError); }
	return pResult;
}

XRT_API ptr xdom_create_document(void) { return xrtXmlDomCreateDocument(); }
XRT_API ptr xdom_adopt(ptr pHandle) { return pHandle; }
XRT_API void xdom_release(ptr pHandle) { xrtXmlDomNodeRelease((xrtxmldomnode*)pHandle); }
XRT_API int64 xdom_type(ptr pHandle) { return xrtXmlDomNodeType((xrtxmldomnode*)pHandle); }
XRT_API ptr xdom_type_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeTypeName((xrtxmldomnode*)pHandle)); }
XRT_API ptr xdom_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeName((xrtxmldomnode*)pHandle)); }
XRT_API ptr xdom_local_name(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeLocalName((xrtxmldomnode*)pHandle)); }
XRT_API ptr xdom_namespace_uri(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeNamespaceUri((xrtxmldomnode*)pHandle)); }
XRT_API ptr xdom_prefix(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodePrefix((xrtxmldomnode*)pHandle)); }
XRT_API ptr xdom_value(ptr pHandle) { return __xdomCopy((const char*)xrtXmlDomNodeValue((xrtxmldomnode*)pHandle)); }
XRT_API bool xdom_set_value(ptr pHandle, const char* sValue) { return xrtXmlDomNodeSetValue((xrtxmldomnode*)pHandle, (str)sValue); }
XRT_API ptr xdom_text_content(ptr pHandle) { return xrtXmlDomTextContent((xrtxmldomnode*)pHandle); }
XRT_API ptr xdom_parent(ptr pHandle) { return xrtXmlDomNodeParent((xrtxmldomnode*)pHandle); }
XRT_API int64 xdom_child_count(ptr pHandle) { return xrtXmlDomNodeChildCount((xrtxmldomnode*)pHandle); }
XRT_API ptr xdom_child(ptr pHandle, uint32 iIndex) { return xrtXmlDomNodeChild((xrtxmldomnode*)pHandle, iIndex); }
XRT_API ptr xdom_document_element(ptr pHandle) { return xrtXmlDomDocumentElement((xrtxmldomnode*)pHandle); }
XRT_API bool xdom_append_child(ptr pParent, ptr pChild) { return xrtXmlDomAppendChild((xrtxmldomnode*)pParent, (xrtxmldomnode*)pChild); }
XRT_API bool xdom_remove_child(ptr pParent, ptr pChild) { return xrtXmlDomRemoveChild((xrtxmldomnode*)pParent, (xrtxmldomnode*)pChild); }

XRT_API ptr xdom_create_element(ptr pDocument, const char* sName) { return xrtXmlDomCreateElement((xrtxmldomnode*)pDocument, (str)sName); }
XRT_API ptr xdom_create_text(ptr pDocument, const char* sText) { return xrtXmlDomCreateText((xrtxmldomnode*)pDocument, (str)sText); }
XRT_API ptr xdom_create_comment(ptr pDocument, const char* sText) { return xrtXmlDomCreateComment((xrtxmldomnode*)pDocument, (str)sText); }
XRT_API ptr xdom_create_pi(ptr pDocument, const char* sTarget, const char* sText) { return xrtXmlDomCreateProcessingInstruction((xrtxmldomnode*)pDocument, (str)sTarget, (str)sText); }

XRT_API ptr xdom_get_attribute(ptr pHandle, const char* sName)
{
	str sValue = xrtXmlDomGetAttribute((xrtxmldomnode*)pHandle, (str)sName);
	return sValue != NULL ? __xdomCopy((const char*)sValue) : NULL;
}

XRT_API bool xdom_set_attribute(ptr pHandle, const char* sName, const char* sValue) { return xrtXmlDomSetAttribute((xrtxmldomnode*)pHandle, (str)sName, (str)sValue); }
XRT_API bool xdom_remove_attribute(ptr pHandle, const char* sName) { return xrtXmlDomRemoveAttribute((xrtxmldomnode*)pHandle, (str)sName); }

XRT_API xvalue* xdom_attributes(ptr pHandle)
{
	xrtxmldomnode* pNode = (xrtxmldomnode*)pHandle;
	xvalue* pResult = xrtValueObject();
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	for ( i = 0u; i < xrtXmlDomAttributeCount(pNode); i++ ) {
		str sName = xrtXmlDomAttributeName(pNode, i);
		str sValue = xrtXmlDomAttributeValue(pNode, i);
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

static void __xdomWriteOptions(xrtxmldomwriteoptions* pOptions,
	bool bPretty, uint32 iIndent, bool bDeclaration, uint64 iMaxOutput)
{
	xrtXmlDomWriteOptionsInit(pOptions);
	pOptions->bPretty = bPretty;
	pOptions->iIndent = iIndent;
	pOptions->bXmlDeclaration = bDeclaration;
	pOptions->iMaxOutputBytes = iMaxOutput;
}

XRT_API ptr xdom_stringify(ptr pHandle, bool bPretty, uint32 iIndent,
	bool bDeclaration, unsigned long long iMaxOutput)
{
	xrtxmldomwriteoptions tOptions;
	xrtxmlerror* pError = __xdomError();
	ptr pResult;
	__xdomWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xdomClearError(pError);
	pResult = xrtXmlDomStringify((xrtxmldomnode*)pHandle, &tOptions, pError);
	if ( pResult == NULL ) { __xdomPublishError(pError); }
	return pResult;
}

/* 文本缓冲所有权转移给调用方，由 xrtFree 释放。 */
XRT_API ptr xdom_stringify_text(ptr pHandle, bool bPretty, uint32 iIndent,
	bool bDeclaration, unsigned long long iMaxOutput)
{
	xbuffer* pResult = (xbuffer*)xdom_stringify(pHandle, bPretty,
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

XRT_API bool xdom_save(ptr pHandle, const char* sPath, bool bPretty,
	uint32 iIndent, bool bDeclaration, unsigned long long iMaxOutput)
{
	xrtxmldomwriteoptions tOptions;
	xrtxmlerror* pError = __xdomError();
	bool bResult;
	__xdomWriteOptions(&tOptions, bPretty, iIndent, bDeclaration, iMaxOutput);
	__xdomClearError(pError);
	bResult = xrtXmlDomSave((str)sPath, (xrtxmldomnode*)pHandle, &tOptions, pError);
	if ( !bResult ) { __xdomPublishError(pError); }
	return bResult;
}

XRT_API int64 xdom_error_code(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? p->iCode : XRT_XML_ERROR_MEMORY; }
XRT_API int64 xdom_error_line(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iLine : 0; }
XRT_API int64 xdom_error_column(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iColumn : 0; }
XRT_API int64 xdom_error_offset(void) { xrtxmlerror* p = __xdomError(); return p != NULL ? (int64)p->iOffset : 0; }
XRT_API ptr xdom_error_message(void) { xrtxmlerror* p = __xdomError(); return __xdomCopy(p != NULL ? p->sMessage : "XML DOM state is unavailable"); }
