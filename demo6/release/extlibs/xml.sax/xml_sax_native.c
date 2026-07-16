#include "include/xrt_xml.h"

#include <stdio.h>
#include <string.h>



typedef struct {
	xrtxmlerror tError;
	int iNextStatus;
} xlangsaxstate;

static xlangsaxstate* __xlangSaxState(void)
{
	return (xlangsaxstate*)xrtThreadLocalGetOrCreate("xlang.xml.sax.error.v1",
		sizeof(xlangsaxstate));
}

static void __xlangSaxStoreError(const xrtxmlerror* pError, int iStatus)
{
	xlangsaxstate* pState = __xlangSaxState();
	if ( pState == NULL ) { return; }
	if ( pError != NULL ) { pState->tError = *pError; }
	else { xrtXmlErrorInit(&pState->tError); }
	pState->iNextStatus = iStatus;
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

static bool __xlangSaxSetText(xvalue pTable, const char* sKey,
	const char* sValue)
{
	return xvoTableSetText(pTable, sKey, (uint32)strlen(sKey),
		(ptr)(sValue != NULL ? sValue : ""),
		(uint32)strlen(sValue != NULL ? sValue : ""), FALSE);
}

static xvalue __xlangSaxEventValue(const xrtxmlevent* pEvent)
{
	xvalue pResult = xvoCreateTable();
	xvalue pAttributes = NULL;
	xvalue pAttributeList = NULL;
	uint32 i;
	if ( pResult == NULL ) { return NULL; }
	if ( !__xlangSaxSetText(pResult, "type", __xlangSaxEventName(pEvent->iType)) ||
		!__xlangSaxSetText(pResult, "name", (const char*)pEvent->sName) ||
		!__xlangSaxSetText(pResult, "localName", (const char*)pEvent->sLocalName) ||
		!__xlangSaxSetText(pResult, "namespaceUri", (const char*)pEvent->sNamespaceUri) ||
		!__xlangSaxSetText(pResult, "prefix", (const char*)pEvent->sPrefix) ||
		!__xlangSaxSetText(pResult, "text", (const char*)pEvent->sText) ||
		!__xlangSaxSetText(pResult, "target", (const char*)pEvent->sTarget) ||
		!xvoTableSetInt(pResult, "depth", 5u, pEvent->iDepth) ||
		!xvoTableSetInt(pResult, "line", 4u, (int64)pEvent->iLine) ||
		!xvoTableSetInt(pResult, "column", 6u, (int64)pEvent->iColumn) ||
		!xvoTableSetInt(pResult, "offset", 6u, (int64)pEvent->iOffset) ) {
		xvoUnref(pResult);
		return NULL;
	}
	pAttributes = xvoCreateTable();
	pAttributeList = xvoCreateArray();
	if ( pAttributes == NULL || pAttributeList == NULL ) { goto failed; }
	for ( i = 0u; i < pEvent->iAttributeCount; i++ ) {
		const xrtxmlattribute* pItem = &pEvent->pAttributes[i];
		xvalue pDescription = xvoCreateTable();
		if ( pDescription == NULL ||
			!xvoTableSetText(pAttributes, pItem->sName,
				(uint32)strlen((const char*)pItem->sName), pItem->sValue,
				(uint32)strlen((const char*)pItem->sValue), FALSE) ||
			!__xlangSaxSetText(pDescription, "name", (const char*)pItem->sName) ||
			!__xlangSaxSetText(pDescription, "localName", (const char*)pItem->sLocalName) ||
			!__xlangSaxSetText(pDescription, "namespaceUri", (const char*)pItem->sNamespaceUri) ||
			!__xlangSaxSetText(pDescription, "prefix", (const char*)pItem->sPrefix) ||
			!__xlangSaxSetText(pDescription, "value", (const char*)pItem->sValue) ||
			!xvoArrayAppendValue(pAttributeList, pDescription, TRUE) ) {
			xvoUnref(pDescription);
			goto failed;
		}
	}
	if ( !xvoTableSetValue(pResult, "attributes", 10u, pAttributes, TRUE) ||
		!xvoTableSetValue(pResult, "attributeList", 13u, pAttributeList, TRUE) ) {
		goto failed;
	}
	return pResult;

failed:
	xvoUnref(pAttributes);
	xvoUnref(pAttributeList);
	xvoUnref(pResult);
	return NULL;
}

XXAPI ptr xsax_open_memory(ptr pData, uint64 iSize, bool bNamespaceAware,
	bool bPreserveComments, bool bAllowDtd, bool bAllowExternalEntities,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlsax* pParser;
	__xlangSaxOptions(&tOptions, bNamespaceAware, bPreserveComments, bAllowDtd,
		bAllowExternalEntities, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	pParser = xrtXmlSaxOpenMemory(pData, iSize, &tOptions, &tError);
	__xlangSaxStoreError(pParser != NULL ? NULL : &tError,
		pParser != NULL ? XRT_XML_NEXT_EVENT : XRT_XML_NEXT_ERROR);
	return pParser;
}

XXAPI ptr xsax_open_file(str sPath, bool bNamespaceAware,
	bool bPreserveComments, bool bAllowDtd, bool bAllowExternalEntities,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtxmloptions tOptions;
	xrtxmlerror tError;
	xrtxmlsax* pParser;
	__xlangSaxOptions(&tOptions, bNamespaceAware, bPreserveComments, bAllowDtd,
		bAllowExternalEntities, iMaxInput, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth);
	pParser = xrtXmlSaxOpenFile(sPath, &tOptions, &tError);
	__xlangSaxStoreError(pParser != NULL ? NULL : &tError,
		pParser != NULL ? XRT_XML_NEXT_EVENT : XRT_XML_NEXT_ERROR);
	return pParser;
}

XXAPI xvalue xsax_next(ptr pHandle)
{
	const xrtxmlevent* pEvent;
	xrtxmlerror tError;
	int iStatus = xrtXmlSaxNext((xrtxmlsax*)pHandle, &pEvent, &tError);
	xvalue pResult = NULL;
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
	return pResult;
}

XXAPI void xsax_destroy(ptr pHandle)
{
	xrtXmlSaxDestroy((xrtxmlsax*)pHandle);
}

XXAPI ptr xsax_retain(ptr pHandle)
{
	return xrtXmlSaxRetain((xrtxmlsax*)pHandle);
}

XXAPI int64 xsax_status(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	return pState != NULL ? pState->iNextStatus : XRT_XML_NEXT_ERROR;
}

XXAPI int64 xsax_error_code(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	return pState != NULL ? pState->tError.iCode : XRT_XML_ERROR_MEMORY;
}

XXAPI int64 xsax_error_line(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	return pState != NULL ? (int64)pState->tError.iLine : 0;
}

XXAPI int64 xsax_error_column(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	return pState != NULL ? (int64)pState->tError.iColumn : 0;
}

XXAPI int64 xsax_error_offset(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	return pState != NULL ? (int64)pState->tError.iOffset : 0;
}

XXAPI str xsax_error_message(void)
{
	xlangsaxstate* pState = __xlangSaxState();
	const char* sMessage = pState != NULL ? pState->tError.sMessage :
		"XML SAX state is unavailable";
	return xrtCopyStr((str)sMessage, strlen(sMessage));
}
