#include "xrt_netrc_internal.h"

static xrtnetrcoptions __xrtNetrcWriteOptions(const xrtnetrcoptions* pOptions)
{
	xrtnetrcoptions tResult;
	xrtNetrcOptionsInit(&tResult);
	if ( pOptions != NULL && pOptions->iSize >= sizeof(*pOptions) &&
		pOptions->iVersion == XRT_NETRC_OPTIONS_VERSION ) { tResult = *pOptions; }
	return tResult;
}

static bool __xrtNetrcAppend(xbuffer* pOutput, const void* pData, size_t iSize,
	xrt_netrc_context* pContext)
{
	uint64 iLimit;
	if ( pOutput == NULL || pContext == NULL ) { return false; }
	iLimit = pContext->Options.tLimits.iMaxOutputBytes;
	if ( (iLimit != 0u && ((uint64)pOutput->Size > iLimit ||
		(uint64)iSize > iLimit - pOutput->Size)) ||
		!xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			0u, 0u, 0u, "netrc output exceeds the configured limit");
		return false;
	}
	return true;
}

static bool __xrtNetrcAppendText(xbuffer* pOutput, const char* sText,
	xrt_netrc_context* pContext)
{
	return __xrtNetrcAppend(pOutput, sText, sText != NULL ? strlen(sText) : 0u,
		pContext);
}

static bool __xrtNetrcTokenNeedsQuote(const char* sText, size_t iSize)
{
	size_t i;
	if ( iSize == 0u ) { return true; }
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c <= 0x20u || c == 0x7fu || c == '#' || c == '"' ||
			c == '\'' || c == '\\' ) { return true; }
	}
	return false;
}

static bool __xrtNetrcAppendToken(xbuffer* pOutput, const char* sText,
	size_t iSize, xrt_netrc_context* pContext)
{
	size_t i;
	if ( sText == NULL || (pContext->Options.tLimits.iMaxItemBytes != 0u &&
		(uint64)iSize > pContext->Options.tLimits.iMaxItemBytes) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			0u, 0u, 0u, "netrc token exceeds the configured item limit");
		return false;
	}
	if ( !__xrtNetrcTokenNeedsQuote(sText, iSize) ) {
		return __xrtNetrcAppend(pOutput, sText, iSize, pContext);
	}
	if ( !__xrtNetrcAppendText(pOutput, "\"", pContext) ) { return false; }
	for ( i = 0u; i < iSize; ++i ) {
		if ( (sText[i] == '"' || sText[i] == '\\') &&
			!__xrtNetrcAppendText(pOutput, "\\", pContext) ) { return false; }
		if ( !__xrtNetrcAppend(pOutput, &sText[i], 1u, pContext) ) { return false; }
	}
	return __xrtNetrcAppendText(pOutput, "\"", pContext);
}

static bool __xrtNetrcAppendAuthField(xbuffer* pOutput, xvalue* pAuth,
	const char* sKey, xrt_netrc_context* pContext)
{
	xstrview Value;
	xvalue* pValue = xrtValueObjectGet(pAuth, xrtStrView(sKey));
	if ( pValue == NULL || xrtValueType(pValue) == XVALUE_NULL ) { return true; }
	if ( !xrtValueGetString(pValue, &Value) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "netrc authentication values must be strings or null");
		return false;
	}
	return __xrtNetrcAppendText(pOutput, "  ", pContext) &&
		__xrtNetrcAppendText(pOutput, sKey, pContext) &&
		__xrtNetrcAppendText(pOutput, " ", pContext) &&
		__xrtNetrcAppendToken(pOutput, Value.Data, Value.Size, pContext) &&
		__xrtNetrcAppendText(pOutput, "\n", pContext);
}

typedef struct {
	xbuffer* Output;
	xrt_netrc_context* Context;
	bool Failed;
} xrt_netrc_write_context;

static bool __xrtNetrcWriteHost(xstrview Key, xvalue* pAuth,
	xrt_netrc_write_context* pWalk)
{
	bool bDefault = Key.Size == 7u && memcmp(Key.Data, "default", 7u) == 0;
	if ( pWalk->Failed ) { return true; }
	if ( xrtValueType(pAuth) != XVALUE_OBJECT ||
		!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcAppendText(pWalk->Output,
			bDefault ? "default\n" : "machine ", pWalk->Context) ||
		(!bDefault && (!__xrtNetrcAppendToken(pWalk->Output,
			Key.Data, Key.Size, pWalk->Context) ||
			!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context))) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "login", pWalk->Context) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "account", pWalk->Context) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "password", pWalk->Context) ||
		!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) {
		if ( pWalk->Context->Error->iCode == XRT_NETRC_OK ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
				0u, 0u, 0u, "invalid netrc host entry");
		}
		pWalk->Failed = true;
	}
	return !pWalk->Failed;
}

static bool __xrtNetrcWriteMacro(xstrview Key, xvalue* pLines,
	xrt_netrc_write_context* pWalk)
{
	size_t i;
	if ( pWalk->Failed ) { return true; }
	if ( xrtValueType(pLines) != XVALUE_ARRAY ||
		!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcAppendText(pWalk->Output, "macdef ", pWalk->Context) ||
		!__xrtNetrcAppendToken(pWalk->Output, Key.Data, Key.Size,
			pWalk->Context) ||
		!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) {
		goto fail;
	}
	for ( i = 0u; i < xrtValueCount(pLines); ++i ) {
		xstrview Line;
		xvalue* pLine = xrtValueArrayGet(pLines, i);
		if ( !xrtValueGetString(pLine, &Line) ||
			!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
			!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ) { goto fail; }
		if ( memchr(Line.Data, '\r', Line.Size) != NULL ||
			memchr(Line.Data, '\n', Line.Size) != NULL ||
			(pWalk->Context->Options.tLimits.iMaxItemBytes != 0u &&
			 (uint64)Line.Size > pWalk->Context->Options.tLimits.iMaxItemBytes) ||
			!__xrtNetrcAppend(pWalk->Output, Line.Data, Line.Size, pWalk->Context) ||
			!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) { goto fail; }
	}
	if ( !__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) { goto fail; }
	return true;

fail:
	if ( pWalk->Context->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "invalid netrc macro entry");
	}
	pWalk->Failed = true;
	return false;
}

static bool __xrtNetrcWriteObject(xvalue* pObject,
	bool (*pWriter)(xstrview, xvalue*, xrt_netrc_write_context*),
	xrt_netrc_write_context* pWalk)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pValue;

	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pObject, &Iterator) ) {
		return false;
	}
	while ( (pValue = xrtValueIterNext(&Iterator, &Key)) != NULL ) {
		if ( Key.Type != XVALUE_KEY_STRING ||
			!pWriter(Key.String, pValue, pWalk) ) {
			xrtValueIterEnd(&Iterator);
			return false;
		}
	}
	xrtValueIterEnd(&Iterator);
	return true;
}

xbuffer* xrtNetrcStringify(xvalue* pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrt_netrc_context tContext;
	xrt_netrc_write_context tWalk;
	xvalue* pHosts = NULL;
	xvalue* pMacros = NULL;
	xbuffer* pOutput = NULL;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtNetrcErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Options = __xrtNetrcWriteOptions(pOptions);
	tContext.Error = pError;
	if ( pDocument == NULL || xrtValueType(pDocument) != XVALUE_OBJECT ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_TYPE, 0u, 0u, 0u,
			"netrc document must be a dict");
		return NULL;
	}
	pHosts = xrtValueObjectGet(pDocument, xrtStrView("hosts"));
	pMacros = xrtValueObjectGet(pDocument, xrtStrView("macros"));
	if ( xrtValueType(pHosts) != XVALUE_OBJECT ||
		xrtValueType(pMacros) != XVALUE_OBJECT ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_TYPE, 0u, 0u, 0u,
			"netrc document requires hosts and macros dicts");
		return NULL;
	}
	pOutput = xrtBufferCreate();
	if ( pOutput == NULL || !xrtBufferReserve(pOutput, 512u) ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate netrc output");
		return NULL;
	}
	memset(&tWalk, 0, sizeof(tWalk));
	tWalk.Output = pOutput;
	tWalk.Context = &tContext;
	if ( !__xrtNetrcWriteObject(pHosts, __xrtNetrcWriteHost, &tWalk) ||
		(!tWalk.Failed && !__xrtNetrcWriteObject(pMacros, __xrtNetrcWriteMacro,
			&tWalk)) ) {
		tWalk.Failed = true;
	}
	if ( tWalk.Failed ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}

bool xrtNetrcWrite(xvalue* pDocument, xwriter* pWriter,
	bool bTakeOwnership, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrtnetrcerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && pWriter != NULL &&
		xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
			0u, 0u, 0u, "cannot write netrc stream");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
	return bResult;
}

bool xrtNetrcSave(str sPath, xvalue* pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrtnetrcerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && sPath != NULL &&
		xrtFileWriteAll(sPath, (xbytesview){ pOutput->Data, pOutput->Size });
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
			0u, 0u, 0u, "cannot save netrc file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
