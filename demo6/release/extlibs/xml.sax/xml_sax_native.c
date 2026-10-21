#include "xrt_xml_internal.h"

#include <stdio.h>
#include <string.h>
typedef struct {
	xrtxmlerror tError;
	int iStatus;
} xsaxerrorstate;

static xonce __xsaxErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xsaxErrorKey = NULL;

static bool __xsaxInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xsaxErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xsaxErrorKey != NULL;
}

/* 错误位置属于本次解析，不能只保存在跨调用变化的通用错误对象里。 */
static xsaxerrorstate* __xsaxErrorState(void)
{
	xsaxerrorstate* pState;
	if ( !xrtOnce(&__xsaxErrorOnce, __xsaxInitErrorKey, NULL) ||
		__xsaxErrorKey == NULL ) { return NULL; }
	pState = (xsaxerrorstate*)xrtThreadKeyGet(__xsaxErrorKey);
	if ( pState != NULL ) { return pState; }
	pState = (xsaxerrorstate*)xrtMalloc(sizeof(*pState));
	if ( pState == NULL || !xrtThreadKeySet(__xsaxErrorKey, pState) ) {
		xrtFree(pState);
		return NULL;
	}
	xrtXmlErrorInit(&pState->tError);
	pState->iStatus = XRT_XML_NEXT_END;
	return pState;
}

static void __xlangSaxStoreError(const xrtxmlerror* pError, int iStatus)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	if ( pState != NULL ) {
		pState->iStatus = iStatus;
		if ( pError != NULL ) { pState->tError = *pError; }
		else { xrtXmlErrorInit(&pState->tError); }
	}
	if ( iStatus != XRT_XML_NEXT_ERROR ) { return; }
	xrtSetErrorInfo(pState == NULL || pError == NULL ||
		pError->iCode == XRT_XML_ERROR_MEMORY ? XERR_MEMORY : XERR_VALUE,
		"xlang.extlib.xml.sax",
		pError != NULL ? pError->iCode : XRT_XML_ERROR_MEMORY,
		pError != NULL && pError->sMessage[0] != 0 ?
			pError->sMessage : "XML SAX operation failed");
}

static void __xlangSaxOptions(xrtxmloptions* pOptions, bool bNamespaceAware,
	bool bPreserveComments, bool bAllowDtd, bool bAllowExternalEntities,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtXmlOptionsInit(pOptions);
	pOptions->bNamespaceAware = bNamespaceAware;
	pOptions->bPreserveComments = bPreserveComments;
	pOptions->bAllowDtd = bAllowDtd;
	pOptions->bAllowExternalEntities = bAllowExternalEntities;
	pOptions->tLimits.iMaxInputBytes = iMaxInput;
	pOptions->tLimits.iMaxOutputBytes = iMaxOutput;
	pOptions->tLimits.iMaxItemBytes = iMaxItem;
	pOptions->tLimits.iMaxEntries = iMaxEntries;
	pOptions->tLimits.iMaxNodes = iMaxNodes;
	pOptions->tLimits.iMaxDepth = iMaxDepth;
}

static const char* __xlangSaxEventName(int iType)
{
	switch ( iType ) {
	case XRT_XML_EVENT_START: return "start";
	case XRT_XML_EVENT_END: return "end";
	case XRT_XML_EVENT_TEXT: return "text";
	case XRT_XML_EVENT_COMMENT: return "comment";
	case XRT_XML_EVENT_PROCESSING_INSTRUCTION: return "processingInstruction";
	case XRT_XML_EVENT_START_NAMESPACE: return "startNamespace";
	case XRT_XML_EVENT_END_NAMESPACE: return "endNamespace";
	default: return "unknown";
	}
}

static bool __xlangSaxSetText(xvalue* pTable, const char* sKey,
	const char* sValue)
{
	xvalue* pText = xrtValueString(xrtStrView(
		sValue != NULL ? sValue : ""));
	if ( pText == NULL ) { return false; }
	if ( xrtValueObjectSetTake(pTable, xrtStrView(sKey), &pText) ) {
		return true;
	}
	xrtValueRelease(pText);
	return false;
}

static bool __xlangSaxSetInt(xvalue* pTable, const char* sKey, int64 iValue)
{
	xvalue* pNumber = xrtValueInt(iValue);
	if ( pNumber == NULL ) { return false; }
	if ( xrtValueObjectSetTake(pTable, xrtStrView(sKey), &pNumber) ) {
		return true;
	}
	xrtValueRelease(pNumber);
	return false;
}

static xvalue* __xlangSaxEventValue(const xrtxmlevent* pEvent)
{
	xvalue* pResult = xrtValueObject();
	xvalue* pAttributes = NULL;
	xvalue* pAttributeList = NULL;
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	if ( !__xlangSaxSetText(pResult, "type", __xlangSaxEventName(pEvent->iType)) ||
		!__xlangSaxSetText(pResult, "name", (const char*)pEvent->sName) ||
		!__xlangSaxSetText(pResult, "localName", (const char*)pEvent->sLocalName) ||
		!__xlangSaxSetText(pResult, "namespaceUri", (const char*)pEvent->sNamespaceUri) ||
		!__xlangSaxSetText(pResult, "prefix", (const char*)pEvent->sPrefix) ||
		!__xlangSaxSetText(pResult, "text", (const char*)pEvent->sText) ||
		!__xlangSaxSetText(pResult, "target", (const char*)pEvent->sTarget) ||
		!__xlangSaxSetInt(pResult, "depth", pEvent->iDepth) ||
		!__xlangSaxSetInt(pResult, "line", (int64)pEvent->iLine) ||
		!__xlangSaxSetInt(pResult, "column", (int64)pEvent->iColumn) ||
		!__xlangSaxSetInt(pResult, "offset", (int64)pEvent->iOffset) ) {
		xrtValueRelease(pResult);
		return NULL;
	}
	pAttributes = xrtValueObject();
	pAttributeList = xrtValueArray();
	if ( pAttributes == NULL || pAttributeList == NULL ) { goto failed; }
	for ( i = 0u; i < pEvent->iAttributeCount; i++ ) {
		const xrtxmlattribute* pItem = &pEvent->pAttributes[i];
		xvalue* pDescription = xrtValueObject();
		xvalue* pAttributeValue = xrtValueString(xrtStrView(
			(const char*)(pItem->sValue != NULL ? pItem->sValue : (str)"")));
		if ( pDescription == NULL ||
			pAttributeValue == NULL || !xrtValueObjectSetTake(pAttributes,
				xrtStrView((const char*)pItem->sName), &pAttributeValue) ||
			!__xlangSaxSetText(pDescription, "name", (const char*)pItem->sName) ||
			!__xlangSaxSetText(pDescription, "localName", (const char*)pItem->sLocalName) ||
			!__xlangSaxSetText(pDescription, "namespaceUri", (const char*)pItem->sNamespaceUri) ||
			!__xlangSaxSetText(pDescription, "prefix", (const char*)pItem->sPrefix) ||
			!__xlangSaxSetText(pDescription, "value", (const char*)pItem->sValue) ||
			!xrtValueArrayAppendTake(pAttributeList, &pDescription) ) {
			xrtValueRelease(pAttributeValue);
			xrtValueRelease(pDescription);
			goto failed;
		}
	}
	if ( !xrtValueObjectSetTake(pResult, xrtStrView("attributes"), &pAttributes) ||
		!xrtValueObjectSetTake(pResult, xrtStrView("attributeList"), &pAttributeList) ) {
		goto failed;
	}
	return pResult;

failed:
	xrtValueRelease(pAttributes);
	xrtValueRelease(pAttributeList);
	xrtValueRelease(pResult);
	return NULL;
}

XRT_API void* xsax_open_memory(void* pData, unsigned long long iSize, bool bNamespaceAware,
	bool bPreserveComments, bool bAllowDtd, bool bAllowExternalEntities,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, unsigned int iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlsax* pParser;
	__xlangSaxOptions(&tOptions, bNamespaceAware, bPreserveComments, bAllowDtd,
		bAllowExternalEntities, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	xrtClearError();
	pParser = xrtXmlSaxOpenMemory(pData, iSize, &tOptions, &tError);
	__xlangSaxStoreError(pParser != NULL ? NULL : &tError,
		pParser != NULL ? XRT_XML_NEXT_EVENT : XRT_XML_NEXT_ERROR);
	return pParser;
}

XRT_API void* xsax_open_file(const char* sPath, bool bNamespaceAware,
	bool bPreserveComments, bool bAllowDtd, bool bAllowExternalEntities,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, unsigned int iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlsax* pParser;
	__xlangSaxOptions(&tOptions, bNamespaceAware, bPreserveComments, bAllowDtd,
		bAllowExternalEntities, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	xrtClearError();
	pParser = xrtXmlSaxOpenFile((str)sPath, &tOptions, &tError);
	__xlangSaxStoreError(pParser != NULL ? NULL : &tError,
		pParser != NULL ? XRT_XML_NEXT_EVENT : XRT_XML_NEXT_ERROR);
	return pParser;
}

XRT_API xvalue* xsax_next(ptr pHandle)
{
	const xrtxmlevent* pEvent;
	xrtxmlerror tError;
	int iStatus;
	xvalue* pResult = NULL;
	xrtClearError();
	iStatus = xrtXmlSaxNext((xrtxmlsax*)pHandle, &pEvent, &tError);
	if ( iStatus == XRT_XML_NEXT_EVENT ) {
		pResult = __xlangSaxEventValue(pEvent);
		if ( pResult == NULL ) {
			xrtXmlErrorInit(&tError);
			tError.iCode = XRT_XML_ERROR_MEMORY;
			snprintf(tError.sMessage, sizeof(tError.sMessage),
				"could not create xlang SAX event");
			iStatus = XRT_XML_NEXT_ERROR;
		}
	}
	__xlangSaxStoreError(iStatus == XRT_XML_NEXT_ERROR ? &tError : NULL, iStatus);
	/*
	    xlang 的 var 空值由 xrtValueNull() 表示，不是 C 的 NULL 指针。
	    EOF 必须返回这个规范空值；否则 xlang 的 result == null 不成立，
	    迭代协议会继续读取并最终破坏临时作用域栈。
	*/
	return pResult != NULL ? pResult : xrtValueNull();
}

XRT_API bool xsax_close(ptr pHandle)
{
	xrtxmlerror tError;
	bool bClosed;
	xrtClearError();
	bClosed = xrtXmlSaxClose((xrtxmlsax*)pHandle, &tError);
	__xlangSaxStoreError(bClosed ? NULL : &tError,
		bClosed ? XRT_XML_NEXT_END : XRT_XML_NEXT_ERROR);
	return bClosed;
}

XRT_API void xsax_destroy(ptr pHandle)
{
	xrtXmlSaxDestroy((xrtxmlsax*)pHandle);
}

XRT_API ptr xsax_retain(ptr pHandle)
{
	return xrtXmlSaxRetain((xrtxmlsax*)pHandle);
}

XRT_API int64 xsax_status(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	return pState != NULL ? pState->iStatus : XRT_XML_NEXT_ERROR;
}

XRT_API int64 xsax_error_code(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	return pState != NULL ? pState->tError.iCode : XRT_XML_ERROR_MEMORY;
}

XRT_API int64 xsax_error_line(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	return pState != NULL ? (int64)pState->tError.iLine : 0;
}

XRT_API int64 xsax_error_column(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	return pState != NULL ? (int64)pState->tError.iColumn : 0;
}

XRT_API int64 xsax_error_offset(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	return pState != NULL ? (int64)pState->tError.iOffset : 0;
}

XRT_API void* xsax_error_message(void)
{
	xsaxerrorstate* pState = __xsaxErrorState();
	const char* sMessage = pState != NULL && pState->tError.sMessage[0] != 0 ?
		pState->tError.sMessage : "XML SAX operation failed";
	return xrtStrDup(sMessage);
}
