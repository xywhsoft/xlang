#include "xrt_xml_internal.h"



void __xrtXmlCopyParserError(const xrtxmlsax* pParser, xrtxmlerror* pError);

static bool __xrtXmlCountNode(xrtxmlsax* pParser)
{
	if ( pParser->iNodes >= pParser->tOptions.tLimits.iMaxNodes ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
			"XML nodes exceed the configured resource limit");
		return false;
	}
	pParser->iNodes++;
	return true;
}

static void XMLCALL __xrtXmlStart(void* pUser, const XML_Char* sName,
	const XML_Char** pAttributes)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( pParser->iDepth >= pParser->tOptions.tLimits.iMaxDepth ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
			"XML nesting exceeds the configured depth limit");
		return;
	}
	if ( !__xrtXmlCountNode(pParser) ||
		!__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_START) ||
		!__xrtXmlSetEventName(pParser, sName) ||
		!__xrtXmlSetAttributes(pParser, pAttributes) ) { return; }
	pParser->iDepth++;
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlEnd(void* pUser, const XML_Char* sName)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( pParser->iDepth != 0u ) { pParser->iDepth--; }
	if ( !__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_END) ||
		!__xrtXmlSetEventName(pParser, sName) ) { return; }
	pParser->tEvent.iDepth = pParser->iDepth;
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlText(void* pUser, const XML_Char* sText, int iSize)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( iSize == 0 || !__xrtXmlCountNode(pParser) ||
		!__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_TEXT) ||
		!__xrtXmlSetEventText(pParser, sText, iSize) ) { return; }
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlComment(void* pUser, const XML_Char* sText)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( !pParser->tOptions.bPreserveComments ) { return; }
	if ( !__xrtXmlCountNode(pParser) ||
		!__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_COMMENT) ||
		!__xrtXmlSetEventText(pParser, sText, (int)strlen(sText)) ) { return; }
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlPi(void* pUser, const XML_Char* sTarget,
	const XML_Char* sData)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( !__xrtXmlCountNode(pParser) ||
		!__xrtXmlPrepareEvent(pParser,
			XRT_XML_EVENT_PROCESSING_INSTRUCTION) ) { return; }
	pParser->tEvent.sTarget = xrtStrDup(sTarget);
	if ( pParser->tEvent.sTarget == NULL ||
		!__xrtXmlSetEventText(pParser, sData, (int)strlen(sData)) ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while copying XML processing instruction");
		return;
	}
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlStartNamespace(void* pUser,
	const XML_Char* sPrefix, const XML_Char* sUri)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( !__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_START_NAMESPACE) ) { return; }
	pParser->tEvent.sPrefix = xrtStrDup(sPrefix != NULL ? sPrefix : "");
	pParser->tEvent.sNamespaceUri = xrtStrDup(sUri != NULL ? sUri : "");
	if ( pParser->tEvent.sPrefix == NULL || pParser->tEvent.sNamespaceUri == NULL ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while copying XML namespace");
		return;
	}
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlEndNamespace(void* pUser, const XML_Char* sPrefix)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	if ( !__xrtXmlPrepareEvent(pParser, XRT_XML_EVENT_END_NAMESPACE) ) { return; }
	pParser->tEvent.sPrefix = xrtStrDup(sPrefix != NULL ? sPrefix : "");
	if ( pParser->tEvent.sPrefix == NULL ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while copying XML namespace");
		return;
	}
	__xrtXmlEmitEvent(pParser);
}

static void XMLCALL __xrtXmlStartDoctype(void* pUser,
	const XML_Char* sName, const XML_Char* sSystemId,
	const XML_Char* sPublicId, int bInternalSubset)
{
	xrtxmlsax* pParser = (xrtxmlsax*)pUser;
	(void)sName; (void)sSystemId; (void)sPublicId; (void)bInternalSubset;
	if ( !pParser->tOptions.bAllowDtd ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_DTD,
			"DTD is disabled by the XML security policy");
	}
}

static int XMLCALL __xrtXmlExternalEntity(XML_Parser pExpat,
	const XML_Char* sContext, const XML_Char* sBase,
	const XML_Char* sSystemId, const XML_Char* sPublicId)
{
	xrtxmlsax* pParser = (xrtxmlsax*)XML_GetUserData(pExpat);
	(void)sContext; (void)sBase; (void)sSystemId; (void)sPublicId;
	__xrtXmlSetError(pParser, XRT_XML_ERROR_EXTERNAL_ENTITY,
		pParser->tOptions.bAllowExternalEntities ?
		"no external entity resolver was provided" :
		"external entities are disabled by the XML security policy");
	return XML_STATUS_ERROR;
}

static bool __xrtXmlValidateOptions(const xrtxmloptions* pOptions,
	xrtxmloptions* pResult)
{
	xrtXmlOptionsInit(pResult);
	if ( pOptions == NULL ) { return true; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_XML_OPTIONS_VERSION ||
		pOptions->tLimits.iSize != sizeof(xrtresourcelimits) ||
		pOptions->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pOptions->tLimits.iReserved != 0u ||
		pOptions->tLimits.iFlags != 0u ) { return false; }
	*pResult = *pOptions;
	/* XRT 限额中的零表示不限制；内部统一成最大值以简化计数检查。 */
	if ( pResult->tLimits.iMaxInputBytes == 0u )
		pResult->tLimits.iMaxInputBytes = UINT64_MAX;
	if ( pResult->tLimits.iMaxOutputBytes == 0u )
		pResult->tLimits.iMaxOutputBytes = UINT64_MAX;
	if ( pResult->tLimits.iMaxItemBytes == 0u )
		pResult->tLimits.iMaxItemBytes = UINT64_MAX;
	if ( pResult->tLimits.iMaxEntries == 0u )
		pResult->tLimits.iMaxEntries = UINT64_MAX;
	if ( pResult->tLimits.iMaxNodes == 0u )
		pResult->tLimits.iMaxNodes = UINT64_MAX;
	if ( pResult->tLimits.iMaxDepth == 0u )
		pResult->tLimits.iMaxDepth = UINT32_MAX;
	return true;
}

/* 已知长度的内存输入在复制或创建 Reader 前执行同一限额。 */
bool __xrtXmlMemoryPreflight(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, bool bSnapshot, xrtxmlerror* pError)
{
	xrtxmloptions tOptions;
	xrtXmlErrorInit(pError);
	if ( (pData == NULL && iSize != 0u) ||
		!__xrtXmlValidateOptions(pOptions, &tOptions) ) {
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_ARGUMENT;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"invalid XML memory input or options");
		}
		return false;
	}
	if ( iSize > SIZE_MAX || (bSnapshot && iSize > UINT32_MAX) ||
		iSize > tOptions.tLimits.iMaxInputBytes ) {
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_LIMIT;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"XML input exceeds the configured limit");
		}
		return false;
	}
	return true;
}

static xrtxmlsax* __xrtXmlCreate(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, bool bPull, xrtxml_event_proc pHandler,
	ptr pContext, xrtxmlerror* pError)
{
	xrtxmlsax* pParser;
	xrtxmloptions tOptions;
	xrtXmlErrorInit(pError);
	if ( pReader == NULL || !__xrtXmlValidateOptions(pOptions, &tOptions) ) {
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_ARGUMENT;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"invalid XML reader or options");
		}
		if ( pReader != NULL && bTakeOwnership ) { xrtReaderDestroy(pReader); }
		return NULL;
	}
	pParser = (xrtxmlsax*)xrtMalloc(sizeof(*pParser));
	if ( pParser == NULL ) {
		if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
		if ( pError != NULL ) { pError->iCode = XRT_XML_ERROR_MEMORY; }
		return NULL;
	}
	memset(pParser, 0, sizeof(*pParser));
	pParser->iReferences = 1u;
	pParser->pReader = pReader;
	pParser->bTakeReader = bTakeOwnership;
	pParser->bPull = bPull;
	pParser->pHandler = pHandler;
	pParser->pHandlerContext = pContext;
	pParser->tOptions = tOptions;
	pParser->pExpat = tOptions.bNamespaceAware ?
		XML_ParserCreateNS(NULL, XRT_XML_NS_SEPARATOR) : XML_ParserCreate(NULL);
	if ( pParser->pExpat == NULL ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"could not create Expat parser");
		__xrtXmlCopyParserError(pParser, pError);
		xrtXmlSaxDestroy(pParser);
		return NULL;
	}
	XML_SetUserData(pParser->pExpat, pParser);
	XML_SetElementHandler(pParser->pExpat, __xrtXmlStart, __xrtXmlEnd);
	XML_SetCharacterDataHandler(pParser->pExpat, __xrtXmlText);
	XML_SetCommentHandler(pParser->pExpat, __xrtXmlComment);
	XML_SetProcessingInstructionHandler(pParser->pExpat, __xrtXmlPi);
	XML_SetStartDoctypeDeclHandler(pParser->pExpat, __xrtXmlStartDoctype);
	XML_SetExternalEntityRefHandler(pParser->pExpat, __xrtXmlExternalEntity);
	XML_SetParamEntityParsing(pParser->pExpat, XML_PARAM_ENTITY_PARSING_NEVER);
	if ( tOptions.bNamespaceAware ) {
		XML_SetReturnNSTriplet(pParser->pExpat, 1);
		XML_SetNamespaceDeclHandler(pParser->pExpat,
			__xrtXmlStartNamespace, __xrtXmlEndNamespace);
	}
#ifdef XML_DTD
	XML_SetBillionLaughsAttackProtectionMaximumAmplification(
		pParser->pExpat, (float)tOptions.tLimits.iMaxCompressionRatio);
	XML_SetBillionLaughsAttackProtectionActivationThreshold(
		pParser->pExpat, (unsigned long long)(tOptions.tLimits.iMaxItemBytes <
			8192u ? tOptions.tLimits.iMaxItemBytes : 8192u));
#endif
	return pParser;
}

/* 保留 Reader 回调报告的内存故障，其他流失败归为 I/O。 */
static int __xrtXmlReaderFailureCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_XML_ERROR_MEMORY : XRT_XML_ERROR_IO;
}

static int __xrtXmlFinish(xrtxmlsax* pParser)
{
	pParser->bDone = true;
	if ( pParser->bPull && pParser->bTakeReader && pParser->pReader != NULL ) {
		xreader* pOwned = pParser->pReader;
		pParser->pReader = NULL;
		pParser->bTakeReader = false;
		if ( !xrtReaderDestroy(pOwned) ) {
			__xrtXmlSetError(pParser, __xrtXmlReaderFailureCode(),
				"could not close XML input");
			return XRT_XML_NEXT_ERROR;
		}
	}
	return XRT_XML_NEXT_END;
}

int __xrtXmlPump(xrtxmlsax* pParser)
{
	enum XML_Status iStatus;
	uint64 iRead;
	if ( pParser == NULL || pParser->bFailed ) { return XRT_XML_NEXT_ERROR; }
	if ( pParser->bDone ) { return XRT_XML_NEXT_END; }
	for (;;) {
		if ( pParser->bSuspended ) {
			pParser->bSuspended = false;
			iStatus = XML_ResumeParser(pParser->pExpat);
			if ( iStatus == XML_STATUS_SUSPENDED ) { return XRT_XML_NEXT_EVENT; }
			if ( iStatus == XML_STATUS_ERROR ) {
				__xrtXmlSetExpatError(pParser);
				return XRT_XML_NEXT_ERROR;
			}
			if ( pParser->bFinalSent ) {
				return __xrtXmlFinish(pParser);
			}
		}
		if ( !xrtReaderRead(pParser->pReader, pParser->aReadBuffer,
			XRT_XML_READ_CHUNK, &iRead) ) {
			__xrtXmlSetError(pParser, __xrtXmlReaderFailureCode(),
				"could not read XML input");
			return XRT_XML_NEXT_ERROR;
		}
		if ( iRead > pParser->tOptions.tLimits.iMaxInputBytes -
			pParser->iInputBytes ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
				"XML input exceeds the configured resource limit");
			return XRT_XML_NEXT_ERROR;
		}
		pParser->iInputBytes += iRead;
		if ( iRead > (uint64)INT_MAX ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
				"XML input chunk is too large");
			return XRT_XML_NEXT_ERROR;
		}
		pParser->bFinalSent = iRead == 0u;
		iStatus = XML_Parse(pParser->pExpat, (const char*)pParser->aReadBuffer,
			(int)iRead, pParser->bFinalSent ? XML_TRUE : XML_FALSE);
		if ( iStatus == XML_STATUS_SUSPENDED ) { return XRT_XML_NEXT_EVENT; }
		if ( iStatus == XML_STATUS_ERROR ) {
			__xrtXmlSetExpatError(pParser);
			return XRT_XML_NEXT_ERROR;
		}
		if ( pParser->bFinalSent ) {
			return __xrtXmlFinish(pParser);
		}
	}
}

XRT_API xrtxmlsax* xrtXmlSaxOpen(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	return __xrtXmlCreate(pReader, bTakeOwnership, pOptions, true, NULL, NULL,
		pError);
}

XRT_API xrtxmlsax* xrtXmlSaxOpenMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xbuffer* pBuffer;
	xreader* pReader;
	if ( !__xrtXmlMemoryPreflight(pData, iSize, pOptions, true, pError) ) { return NULL; }
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL || !xrtBufferReserve(pBuffer,
		(size_t)(iSize < XRT_XML_READ_CHUNK ? iSize : XRT_XML_READ_CHUNK)) ||
		(iSize != 0u && !xrtBufferAppend(pBuffer,
			(xbytesview){ (cbytes)pData, (size_t)iSize })) ) {
		xrtBufferDestroy(pBuffer);
		xrtXmlErrorInit(pError);
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_MEMORY;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"could not copy XML memory input");
		}
		return NULL;
	}
	pReader = xrtReaderTakeBuffer(&pBuffer);
	if ( pReader == NULL ) {
		xrtBufferDestroy(pBuffer);
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_MEMORY;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"could not create XML memory reader");
		}
		return NULL;
	}
	return xrtXmlSaxOpen(pReader, true, pOptions, pError);
}

XRT_API xrtxmlsax* xrtXmlSaxOpenFile(str sPath,
	const xrtxmloptions* pOptions, xrtxmlerror* pError)
{
	xreader* pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		int iCode = __xrtXmlReaderFailureCode();
		xrtXmlErrorInit(pError);
		if ( pError != NULL ) {
			pError->iCode = iCode;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"could not open XML file");
		}
		return NULL;
	}
	return xrtXmlSaxOpen(pReader, true, pOptions, pError);
}

XRT_API int xrtXmlSaxNext(xrtxmlsax* pParser, const xrtxmlevent** ppEvent,
	xrtxmlerror* pError)
{
	int iResult;
	if ( ppEvent != NULL ) { *ppEvent = NULL; }
	if ( pParser == NULL || !pParser->bPull ) { return XRT_XML_NEXT_ERROR; }
	if ( __xrtXmlTakePendingEvent(pParser) ) {
		if ( ppEvent != NULL ) { *ppEvent = &pParser->tEvent; }
		return XRT_XML_NEXT_EVENT;
	}
	xrtXmlEventClear(&pParser->tEvent);
	iResult = __xrtXmlPump(pParser);
	if ( iResult == XRT_XML_NEXT_EVENT ) {
		if ( !__xrtXmlTakePendingEvent(pParser) ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_FORMAT,
				"XML parser suspended without an event");
			iResult = XRT_XML_NEXT_ERROR;
		} else if ( ppEvent != NULL ) {
			*ppEvent = &pParser->tEvent;
		}
	}
	if ( iResult == XRT_XML_NEXT_ERROR ) {
		__xrtXmlCopyParserError(pParser, pError);
	}
	return iResult;
}

XRT_API bool xrtXmlSaxClose(xrtxmlsax* pParser, xrtxmlerror* pError)
{
	uint32 i;
	if ( pParser == NULL || !pParser->bPull ) {
		xrtXmlErrorInit(pError);
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_ARGUMENT;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"invalid XML pull reader");
		}
		return false;
	}
	/* close 之后不能再交付已经缓冲、但尚未被调用方读取的事件。 */
	xrtXmlEventClear(&pParser->tEvent);
	for ( i = 0u; i < pParser->iPendingCount; ++i ) {
		xrtXmlEventClear(&pParser->pPendingEvents[i]);
	}
	pParser->iPendingCount = 0u;
	pParser->iPendingIndex = 0u;
	if ( !pParser->bDone ) { (void)__xrtXmlFinish(pParser); }
	if ( pParser->bFailed ) {
		__xrtXmlCopyParserError(pParser, pError);
		return false;
	}
	xrtXmlErrorInit(pError);
	return true;
}

XRT_API void xrtXmlSaxDestroy(xrtxmlsax* pParser)
{
	uint32 i;
	if ( pParser == NULL ) { return; }
	if ( pParser->iReferences > 1u ) {
		pParser->iReferences--;
		return;
	}
	xrtXmlEventClear(&pParser->tEvent);
	for ( i = 0u; i < pParser->iPendingCount; i++ ) {
		xrtXmlEventClear(&pParser->pPendingEvents[i]);
	}
	xrtFree(pParser->pPendingEvents);
	if ( pParser->pExpat != NULL ) { XML_ParserFree(pParser->pExpat); }
	if ( pParser->bTakeReader && pParser->pReader != NULL ) {
		xrtReaderDestroy(pParser->pReader);
	}
	xrtFree(pParser);
}

XRT_API xrtxmlsax* xrtXmlSaxRetain(xrtxmlsax* pParser)
{
	if ( pParser == NULL || pParser->iReferences == UINT32_MAX ) { return NULL; }
	pParser->iReferences++;
	return pParser;
}

XRT_API bool xrtXmlSaxParse(xreader* pReader, bool bTakeOwnership,
	const xrtxmloptions* pOptions, xrtxml_event_proc pHandler, ptr pContext,
	xrtxmlerror* pError)
{
	xrtxmlsax* pParser;
	bool bClosed = true;
	int iResult;
	if ( pHandler == NULL ) {
		if ( pReader != NULL && bTakeOwnership ) { xrtReaderDestroy(pReader); }
		return false;
	}
	pParser = __xrtXmlCreate(pReader, bTakeOwnership, pOptions, false,
		pHandler, pContext, pError);
	if ( pParser == NULL ) { return false; }
	iResult = __xrtXmlPump(pParser);
	if ( iResult == XRT_XML_NEXT_ERROR ) {
		__xrtXmlCopyParserError(pParser, pError);
	}
	if ( pParser->bTakeReader && pParser->pReader != NULL ) {
		xreader* pOwned = pParser->pReader;
		pParser->pReader = NULL;
		pParser->bTakeReader = false;
		bClosed = xrtReaderDestroy(pOwned);
	}
	if ( !bClosed && iResult == XRT_XML_NEXT_END ) {
		/* 一次性解析成功后，拥有型流的关闭仍可能失败。 */
		__xrtXmlSetError(pParser, __xrtXmlReaderFailureCode(),
			"could not close XML input");
		__xrtXmlCopyParserError(pParser, pError);
	}
	xrtXmlSaxDestroy(pParser);
	return iResult == XRT_XML_NEXT_END && bClosed;
}

XRT_API bool xrtXmlSaxParseMemory(const void* pData, uint64 iSize,
	const xrtxmloptions* pOptions, xrtxml_event_proc pHandler, ptr pContext,
	xrtxmlerror* pError)
{
	xreader* pReader;
	if ( !__xrtXmlMemoryPreflight(pData, iSize, pOptions, false, pError) ) {
		return false;
	}
	pReader = xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize });
	if ( pReader == NULL ) {
		if ( pError != NULL ) {
			pError->iCode = XRT_XML_ERROR_MEMORY;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"could not create XML memory reader");
		}
		return false;
	}
	return xrtXmlSaxParse(pReader, true, pOptions, pHandler, pContext, pError);
}

XRT_API bool xrtXmlSaxParseFile(str sPath, const xrtxmloptions* pOptions,
	xrtxml_event_proc pHandler, ptr pContext, xrtxmlerror* pError)
{
	xreader* pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		int iCode = __xrtXmlReaderFailureCode();
		xrtXmlErrorInit(pError);
		if ( pError != NULL ) {
			pError->iCode = iCode;
			snprintf(pError->sMessage, sizeof(pError->sMessage),
				"could not open XML file");
		}
		return false;
	}
	return xrtXmlSaxParse(pReader, true, pOptions, pHandler, pContext, pError);
}
