#include "xrt_netrc_internal.h"

void xrtNetrcOptionsInit(xrtnetrcoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_NETRC_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->bStrictDuplicates = true;
}

void xrtNetrcErrorInit(xrtnetrcerror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

void __xrtNetrcSetError(xrtnetrcerror* pError, int iCode,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sMessage)
{
	if ( pError == NULL || pError->iCode != XRT_NETRC_OK ) { return; }
	pError->iCode = iCode;
	pError->iLine = iLine;
	pError->iColumn = iColumn;
	pError->iOffset = iOffset;
	if ( sMessage != NULL ) {
		snprintf(pError->sMessage, sizeof(pError->sMessage), "%s", sMessage);
	}
}

bool __xrtNetrcLimitNode(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset)
{
	if ( pContext == NULL ) { return false; }
	pContext->iNodes++;
	if ( pContext->Options.tLimits.iMaxNodes != 0u &&
		pContext->iNodes > pContext->Options.tLimits.iMaxNodes ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			iLine, iColumn, iOffset, "netrc node limit exceeded");
		return false;
	}
	return true;
}

bool __xrtNetrcLimitEntry(xrt_netrc_context* pContext, uint64 iLine,
	uint64 iColumn, uint64 iOffset)
{
	if ( pContext == NULL ) { return false; }
	pContext->iEntries++;
	if ( pContext->Options.tLimits.iMaxEntries != 0u &&
		pContext->iEntries > pContext->Options.tLimits.iMaxEntries ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			iLine, iColumn, iOffset, "netrc entry limit exceeded");
		return false;
	}
	return true;
}

bool __xrtNetrcSetValue(xvalue* pTable, const char* sKey, xvalue* pValue)
{
	if ( pTable == NULL || sKey == NULL || pValue == NULL ) { return false; }
	if ( xrtValueObjectSetTake(pTable, xrtStrView(sKey), &pValue) ) {
		return true;
	}
	xrtValueRelease(pValue);
	return false;
}

xvalue* __xrtNetrcText(const char* sText, size_t iSize)
{
	if ( sText == NULL && iSize != 0u ) { return NULL; }
	return xrtValueString((xstrview){ sText != NULL ? sText : "", iSize });
}
