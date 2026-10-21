#include "xrt_xml_internal.h"



static str __xrtXmlDup(const char* sText, size_t iSize)
{
	return iSize != 0u ? xrtStrDupN(sText != NULL ? sText : "", iSize) :
		xrtStrDup("");
}

static void __xrtXmlFreeAttribute(xrtxmlattribute* pAttribute)
{
	if ( pAttribute == NULL ) { return; }
	xrtFree(pAttribute->sName);
	xrtFree(pAttribute->sLocalName);
	xrtFree(pAttribute->sNamespaceUri);
	xrtFree(pAttribute->sPrefix);
	xrtFree(pAttribute->sValue);
	memset(pAttribute, 0, sizeof(*pAttribute));
}

XRT_API void xrtXmlOptionsInit(xrtxmloptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_XML_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->bNamespaceAware = true;
	pOptions->bPreserveComments = true;
}

XRT_API void xrtXmlErrorInit(xrtxmlerror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

static bool __xrtXmlNameStart(uint32 iScalar)
{
	return iScalar == ':' || iScalar == '_' ||
		(iScalar >= 'A' && iScalar <= 'Z') ||
		(iScalar >= 'a' && iScalar <= 'z') ||
		(iScalar >= 0xc0u && iScalar <= 0xd6u) ||
		(iScalar >= 0xd8u && iScalar <= 0xf6u) ||
		(iScalar >= 0xf8u && iScalar <= 0x2ffu) ||
		(iScalar >= 0x370u && iScalar <= 0x37du) ||
		(iScalar >= 0x37fu && iScalar <= 0x1fffu) ||
		(iScalar >= 0x200cu && iScalar <= 0x200du) ||
		(iScalar >= 0x2070u && iScalar <= 0x218fu) ||
		(iScalar >= 0x2c00u && iScalar <= 0x2fefu) ||
		(iScalar >= 0x3001u && iScalar <= 0xd7ffu) ||
		(iScalar >= 0xf900u && iScalar <= 0xfdcfu) ||
		(iScalar >= 0xfdf0u && iScalar <= 0xfffdu) ||
		(iScalar >= 0x10000u && iScalar <= 0xeffffu);
}

/* XML 1.0 Fifth Edition 的 NameStartChar / NameChar。 */
XRT_API bool xrtXmlNameValid(str sName)
{
	const char* sText = (const char*)sName;
	size_t iPosition = 0u;
	size_t iSize;
	if ( sText == NULL || sText[0] == 0 ) { return false; }
	iSize = strlen(sText);
	while ( iPosition < iSize ) {
		uint32 iScalar;
		size_t iRead = 0u;
		bool bStart;
		if ( xrtUtf8Decode((xstrview){ sText + iPosition,
			iSize - iPosition }, &iScalar, &iRead) != XUTF_OK ||
			iRead == 0u ) { return false; }
		bStart = __xrtXmlNameStart(iScalar);
		if ( !bStart && (iPosition == 0u ||
			(iScalar != '-' && iScalar != '.' &&
			 !(iScalar >= '0' && iScalar <= '9') && iScalar != 0xb7u &&
			 !(iScalar >= 0x300u && iScalar <= 0x36fu) &&
			 !(iScalar >= 0x203fu && iScalar <= 0x2040u))) ) {
			return false;
		}
		iPosition += iRead;
	}
	return true;
}

XRT_API void xrtXmlEventClear(xrtxmlevent* pEvent)
{
	uint32 i;
	if ( pEvent == NULL ) { return; }
	xrtFree(pEvent->sName);
	xrtFree(pEvent->sLocalName);
	xrtFree(pEvent->sNamespaceUri);
	xrtFree(pEvent->sPrefix);
	xrtFree(pEvent->sText);
	xrtFree(pEvent->sTarget);
	for ( i = 0u; i < pEvent->iAttributeCount; i++ ) {
		__xrtXmlFreeAttribute(&pEvent->pAttributes[i]);
	}
	xrtFree(pEvent->pAttributes);
	memset(pEvent, 0, sizeof(*pEvent));
}

static void __xrtXmlCopyError(const xrtxmlsax* pParser, xrtxmlerror* pError)
{
	if ( pError != NULL ) {
		if ( pParser != NULL ) { *pError = pParser->tError; }
		else { xrtXmlErrorInit(pError); }
	}
}

void __xrtXmlSetError(xrtxmlsax* pParser, int iCode, const char* sMessage)
{
	if ( pParser == NULL || pParser->bFailed ) { return; }
	pParser->bFailed = true;
	pParser->tError.iCode = iCode;
	pParser->tError.iLine = pParser->pExpat != NULL ?
		(uint64)XML_GetCurrentLineNumber(pParser->pExpat) : 0u;
	pParser->tError.iColumn = pParser->pExpat != NULL ?
		(uint64)XML_GetCurrentColumnNumber(pParser->pExpat) : 0u;
	pParser->tError.iOffset = pParser->pExpat != NULL ?
		(uint64)XML_GetCurrentByteIndex(pParser->pExpat) : pParser->iInputBytes;
	snprintf(pParser->tError.sMessage, sizeof(pParser->tError.sMessage), "%s",
		sMessage != NULL && sMessage[0] != 0 ? sMessage : "XML parse failed");
	if ( pParser->pExpat != NULL ) { XML_StopParser(pParser->pExpat, XML_FALSE); }
}

void __xrtXmlSetExpatError(xrtxmlsax* pParser)
{
	enum XML_Error iError;
	if ( pParser == NULL || pParser->bFailed || pParser->pExpat == NULL ) { return; }
	iError = XML_GetErrorCode(pParser->pExpat);
	__xrtXmlSetError(pParser, XRT_XML_ERROR_FORMAT, XML_ErrorString(iError));
}

bool __xrtXmlPrepareEvent(xrtxmlsax* pParser, int iType)
{
	if ( pParser == NULL || pParser->bFailed ) { return false; }
	xrtXmlEventClear(&pParser->tEvent);
	pParser->tEvent.iType = iType;
	pParser->tEvent.iDepth = pParser->iDepth;
	pParser->tEvent.iLine = (uint64)XML_GetCurrentLineNumber(pParser->pExpat);
	pParser->tEvent.iColumn = (uint64)XML_GetCurrentColumnNumber(pParser->pExpat);
	pParser->tEvent.iOffset = (uint64)XML_GetCurrentByteIndex(pParser->pExpat);
	return true;
}

bool __xrtXmlEmitEvent(xrtxmlsax* pParser)
{
	bool bSuspend;
	if ( pParser == NULL || pParser->bFailed ) { return false; }
	if ( pParser->pHandler != NULL &&
		!pParser->pHandler(pParser->pHandlerContext, &pParser->tEvent) ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_CALLBACK,
			"XML event callback rejected the event");
		return false;
	}
	if ( pParser->bPull ) {
		xrtxmlevent* pEvents;
		if ( pParser->iPendingCount == pParser->iPendingCapacity ) {
			uint32 iCapacity = pParser->iPendingCapacity != 0u ?
				pParser->iPendingCapacity * 2u : 8u;
			pEvents = (xrtxmlevent*)xrtRealloc(pParser->pPendingEvents,
				sizeof(xrtxmlevent) * iCapacity);
			if ( pEvents == NULL ) {
				__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
					"out of memory while queuing XML event");
				return false;
			}
			memset(pEvents + pParser->iPendingCapacity, 0,
				sizeof(xrtxmlevent) * (iCapacity - pParser->iPendingCapacity));
			pParser->pPendingEvents = pEvents;
			pParser->iPendingCapacity = iCapacity;
		}
		pParser->pPendingEvents[pParser->iPendingCount++] = pParser->tEvent;
		memset(&pParser->tEvent, 0, sizeof(pParser->tEvent));
		bSuspend = pParser->pPendingEvents[pParser->iPendingCount - 1u].iType !=
			XRT_XML_EVENT_START_NAMESPACE &&
			pParser->pPendingEvents[pParser->iPendingCount - 1u].iType !=
			XRT_XML_EVENT_END_NAMESPACE;
		if ( bSuspend && !pParser->bSuspended ) {
			pParser->bSuspended = XML_StopParser(pParser->pExpat, XML_TRUE) ==
				XML_STATUS_OK;
			return pParser->bSuspended;
		}
	}
	return true;
}

bool __xrtXmlTakePendingEvent(xrtxmlsax* pParser)
{
	if ( pParser == NULL || pParser->iPendingIndex >= pParser->iPendingCount ) {
		return false;
	}
	xrtXmlEventClear(&pParser->tEvent);
	pParser->tEvent = pParser->pPendingEvents[pParser->iPendingIndex];
	memset(&pParser->pPendingEvents[pParser->iPendingIndex], 0,
		sizeof(xrtxmlevent));
	pParser->iPendingIndex++;
	if ( pParser->iPendingIndex == pParser->iPendingCount ) {
		pParser->iPendingIndex = 0u;
		pParser->iPendingCount = 0u;
	}
	return true;
}

static bool __xrtXmlSplitName(const char* sName, str* psQualified,
	str* psLocal, str* psUri, str* psPrefix)
{
	const char* pFirst;
	const char* pSecond;
	size_t iLocal;
	size_t iPrefix;
	char* sQualified;
	*psQualified = *psLocal = *psUri = *psPrefix = NULL;
	if ( sName == NULL ) { sName = ""; }
	pFirst = strchr(sName, XRT_XML_NS_SEPARATOR);
	if ( pFirst == NULL ) {
		*psQualified = __xrtXmlDup(sName, strlen(sName));
		*psLocal = __xrtXmlDup(sName, strlen(sName));
		*psUri = __xrtXmlDup("", 0u);
		*psPrefix = __xrtXmlDup("", 0u);
		return *psQualified != NULL && *psLocal != NULL && *psUri != NULL &&
			*psPrefix != NULL;
	}
	pSecond = strchr(pFirst + 1, XRT_XML_NS_SEPARATOR);
	iLocal = (size_t)((pSecond != NULL ? pSecond : sName + strlen(sName)) -
		(pFirst + 1));
	iPrefix = pSecond != NULL ? strlen(pSecond + 1) : 0u;
	*psUri = __xrtXmlDup(sName, (size_t)(pFirst - sName));
	*psLocal = __xrtXmlDup(pFirst + 1, iLocal);
	*psPrefix = __xrtXmlDup(pSecond != NULL ? pSecond + 1 : "", iPrefix);
	sQualified = (char*)xrtMalloc(iPrefix + (iPrefix != 0u ? 1u : 0u) +
		iLocal + 1u);
	if ( sQualified != NULL ) {
		if ( iPrefix != 0u ) {
			memcpy(sQualified, pSecond + 1, iPrefix);
			sQualified[iPrefix] = ':';
		}
		memcpy(sQualified + iPrefix + (iPrefix != 0u ? 1u : 0u),
			pFirst + 1, iLocal);
		sQualified[iPrefix + (iPrefix != 0u ? 1u : 0u) + iLocal] = 0;
	}
	*psQualified = (str)sQualified;
	return *psQualified != NULL && *psLocal != NULL && *psUri != NULL &&
		*psPrefix != NULL;
}

bool __xrtXmlSetEventName(xrtxmlsax* pParser, const XML_Char* sName)
{
	if ( !__xrtXmlSplitName(sName, &pParser->tEvent.sName,
		&pParser->tEvent.sLocalName, &pParser->tEvent.sNamespaceUri,
		&pParser->tEvent.sPrefix) ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while copying XML name");
		return false;
	}
	return true;
}

bool __xrtXmlSetEventText(xrtxmlsax* pParser, const XML_Char* sText,
	int iSize)
{
	uint64 iLength = iSize > 0 ? (uint64)iSize : 0u;
	if ( iLength > pParser->tOptions.tLimits.iMaxItemBytes ||
		iLength > pParser->tOptions.tLimits.iMaxOutputBytes ||
		pParser->iOutputBytes > pParser->tOptions.tLimits.iMaxOutputBytes - iLength ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
			"XML text exceeds the configured resource limit");
		return false;
	}
	pParser->tEvent.sText = __xrtXmlDup(sText, (size_t)iLength);
	if ( pParser->tEvent.sText == NULL ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while copying XML text");
		return false;
	}
	pParser->iOutputBytes += iLength;
	return true;
}

bool __xrtXmlSetAttributes(xrtxmlsax* pParser, const XML_Char** pAttributes)
{
	uint32 iCount = 0u;
	uint32 i;
	if ( pAttributes == NULL ) { return true; }
	while ( pAttributes[iCount * 2u] != NULL ) { iCount++; }
	if ( iCount > pParser->tOptions.tLimits.iMaxEntries ||
		pParser->iAttributes > pParser->tOptions.tLimits.iMaxEntries - iCount ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
			"XML attributes exceed the configured resource limit");
		return false;
	}
	if ( iCount == 0u ) { return true; }
	pParser->tEvent.pAttributes = (xrtxmlattribute*)xrtMalloc(
		sizeof(xrtxmlattribute) * iCount);
	if ( pParser->tEvent.pAttributes == NULL ) {
		__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
			"out of memory while allocating XML attributes");
		return false;
	}
	memset(pParser->tEvent.pAttributes, 0, sizeof(xrtxmlattribute) * iCount);
	pParser->tEvent.iAttributeCount = iCount;
	for ( i = 0u; i < iCount; i++ ) {
		xrtxmlattribute* pItem = &pParser->tEvent.pAttributes[i];
		const char* sValue = pAttributes[i * 2u + 1u];
		uint64 iValueLength = (uint64)strlen(sValue);
		if ( iValueLength > pParser->tOptions.tLimits.iMaxItemBytes ||
			iValueLength > pParser->tOptions.tLimits.iMaxOutputBytes -
			pParser->iOutputBytes ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_LIMIT,
				"XML attribute value exceeds the configured resource limit");
			return false;
		}
		if ( !__xrtXmlSplitName(pAttributes[i * 2u], &pItem->sName,
			&pItem->sLocalName, &pItem->sNamespaceUri, &pItem->sPrefix) ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
				"out of memory while copying XML attribute name");
			return false;
		}
		pItem->sValue = __xrtXmlDup(sValue, (size_t)iValueLength);
		if ( pItem->sValue == NULL ) {
			__xrtXmlSetError(pParser, XRT_XML_ERROR_MEMORY,
				"out of memory while copying XML attribute value");
			return false;
		}
		pParser->iOutputBytes += iValueLength;
	}
	pParser->iAttributes += iCount;
	return true;
}

void __xrtXmlCopyParserError(const xrtxmlsax* pParser, xrtxmlerror* pError)
{
	__xrtXmlCopyError(pParser, pError);
}
