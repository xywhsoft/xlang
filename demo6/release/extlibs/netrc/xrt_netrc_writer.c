#include "xrt_netrc_internal.h"

static xrtnetrcoptions __xrtNetrcWriteOptions(const xrtnetrcoptions* pOptions)
{
	xrtnetrcoptions tResult;
	xrtNetrcOptionsInit(&tResult);
	if ( pOptions != NULL && pOptions->iSize >= sizeof(*pOptions) &&
		pOptions->iVersion == XRT_NETRC_OPTIONS_VERSION ) { tResult = *pOptions; }
	return tResult;
}

static bool __xrtNetrcAppend(xbuffer pOutput, const void* pData, size_t iSize,
	xrt_netrc_context* pContext)
{
	uint64 iLimit;
	if ( pOutput == NULL || pContext == NULL || iSize > UINT32_MAX ) { return FALSE; }
	iLimit = pContext->Options.tLimits.iMaxOutputBytes;
	if ( (iLimit != 0u && ((uint64)pOutput->Length > iLimit ||
		(uint64)iSize > iLimit - pOutput->Length)) ||
		!xrtBufferAppend(pOutput, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			0u, 0u, 0u, "netrc output exceeds the configured limit");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtNetrcAppendText(xbuffer pOutput, const char* sText,
	xrt_netrc_context* pContext)
{
	return __xrtNetrcAppend(pOutput, sText, sText != NULL ? strlen(sText) : 0u,
		pContext);
}

static bool __xrtNetrcTokenNeedsQuote(const char* sText, size_t iSize)
{
	size_t i;
	if ( iSize == 0u ) { return TRUE; }
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c <= 0x20u || c == 0x7fu || c == '#' || c == '"' ||
			c == '\'' || c == '\\' ) { return TRUE; }
	}
	return FALSE;
}

static bool __xrtNetrcAppendToken(xbuffer pOutput, const char* sText,
	size_t iSize, xrt_netrc_context* pContext)
{
	size_t i;
	if ( sText == NULL || (pContext->Options.tLimits.iMaxItemBytes != 0u &&
		(uint64)iSize > pContext->Options.tLimits.iMaxItemBytes) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			0u, 0u, 0u, "netrc token exceeds the configured item limit");
		return FALSE;
	}
	if ( !__xrtNetrcTokenNeedsQuote(sText, iSize) ) {
		return __xrtNetrcAppend(pOutput, sText, iSize, pContext);
	}
	if ( !__xrtNetrcAppendText(pOutput, "\"", pContext) ) { return FALSE; }
	for ( i = 0u; i < iSize; ++i ) {
		if ( (sText[i] == '"' || sText[i] == '\\') &&
			!__xrtNetrcAppendText(pOutput, "\\", pContext) ) { return FALSE; }
		if ( !__xrtNetrcAppend(pOutput, &sText[i], 1u, pContext) ) { return FALSE; }
	}
	return __xrtNetrcAppendText(pOutput, "\"", pContext);
}

static bool __xrtNetrcAppendAuthField(xbuffer pOutput, xvalue pAuth,
	const char* sKey, xrt_netrc_context* pContext)
{
	xvalue pValue = xvoTableGetValue(pAuth, sKey, (uint32)strlen(sKey));
	if ( pValue == NULL || xvoType(pValue) == XVO_DT_NULL ) { return TRUE; }
	if ( xvoType(pValue) != XVO_DT_TEXT ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "netrc authentication values must be strings or null");
		return FALSE;
	}
	return __xrtNetrcAppendText(pOutput, "  ", pContext) &&
		__xrtNetrcAppendText(pOutput, sKey, pContext) &&
		__xrtNetrcAppendText(pOutput, " ", pContext) &&
		__xrtNetrcAppendToken(pOutput, (const char*)xvoGetText(pValue),
			xvoGetSize(pValue), pContext) &&
		__xrtNetrcAppendText(pOutput, "\n", pContext);
}

typedef struct {
	xbuffer Output;
	xrt_netrc_context* Context;
	bool Failed;
} xrt_netrc_write_context;

static bool __xrtNetrcWriteHost(Dict_Key* pKey, ptr pRawValue, ptr pArg)
{
	xrt_netrc_write_context* pWalk = (xrt_netrc_write_context*)pArg;
	xvalue pAuth = *(xvalue*)pRawValue;
	bool bDefault = pKey->KeyLen == 7u &&
		memcmp(pKey->Key, "default", 7u) == 0;
	if ( pWalk->Failed ) { return TRUE; }
	if ( xvoType(pAuth) != XVO_DT_TABLE ||
		!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcAppendText(pWalk->Output,
			bDefault ? "default\n" : "machine ", pWalk->Context) ||
		(!bDefault && (!__xrtNetrcAppendToken(pWalk->Output,
			(const char*)pKey->Key, pKey->KeyLen, pWalk->Context) ||
			!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context))) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "login", pWalk->Context) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "account", pWalk->Context) ||
		!__xrtNetrcAppendAuthField(pWalk->Output, pAuth, "password", pWalk->Context) ||
		!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) {
		if ( pWalk->Context->Error->iCode == XRT_NETRC_OK ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
				0u, 0u, 0u, "invalid netrc host entry");
		}
		pWalk->Failed = TRUE;
	}
	return pWalk->Failed;
}

static bool __xrtNetrcWriteMacro(Dict_Key* pKey, ptr pRawValue, ptr pArg)
{
	xrt_netrc_write_context* pWalk = (xrt_netrc_write_context*)pArg;
	xvalue pLines = *(xvalue*)pRawValue;
	uint32 i;
	if ( pWalk->Failed ) { return TRUE; }
	if ( xvoType(pLines) != XVO_DT_ARRAY ||
		!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ||
		!__xrtNetrcAppendText(pWalk->Output, "macdef ", pWalk->Context) ||
		!__xrtNetrcAppendToken(pWalk->Output, (const char*)pKey->Key,
			pKey->KeyLen, pWalk->Context) ||
		!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) {
		goto fail;
	}
	for ( i = 0u; i < xvoArrayItemCount(pLines); ++i ) {
		xvalue pLine = xvoArrayGetValue(pLines, i);
		const char* sLine;
		size_t iSize;
		if ( xvoType(pLine) != XVO_DT_TEXT ||
			!__xrtNetrcLimitEntry(pWalk->Context, 0u, 0u, 0u) ||
			!__xrtNetrcLimitNode(pWalk->Context, 0u, 0u, 0u) ) { goto fail; }
		sLine = (const char*)xvoGetText(pLine);
		iSize = xvoGetSize(pLine);
		if ( memchr(sLine, '\r', iSize) != NULL || memchr(sLine, '\n', iSize) != NULL ||
			(pWalk->Context->Options.tLimits.iMaxItemBytes != 0u &&
			 (uint64)iSize > pWalk->Context->Options.tLimits.iMaxItemBytes) ||
			!__xrtNetrcAppend(pWalk->Output, sLine, iSize, pWalk->Context) ||
			!__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) { goto fail; }
	}
	if ( !__xrtNetrcAppendText(pWalk->Output, "\n", pWalk->Context) ) { goto fail; }
	return FALSE;

fail:
	if ( pWalk->Context->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "invalid netrc macro entry");
	}
	pWalk->Failed = TRUE;
	return TRUE;
}

xbuffer xrtNetrcStringify(xvalue pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrt_netrc_context tContext;
	xrt_netrc_write_context tWalk;
	xvalue pHosts, pMacros;
	xbuffer pOutput = NULL;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtNetrcErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Options = __xrtNetrcWriteOptions(pOptions);
	tContext.Error = pError;
	if ( pDocument == NULL || xvoType(pDocument) != XVO_DT_TABLE ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_TYPE, 0u, 0u, 0u,
			"netrc document must be a dict");
		return NULL;
	}
	pHosts = xvoTableGetValue(pDocument, "hosts", 5u);
	pMacros = xvoTableGetValue(pDocument, "macros", 6u);
	if ( xvoType(pHosts) != XVO_DT_TABLE || xvoType(pMacros) != XVO_DT_TABLE ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_TYPE, 0u, 0u, 0u,
			"netrc document requires hosts and macros dicts");
		return NULL;
	}
	pOutput = xrtBufferCreate(512u);
	if ( pOutput == NULL ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate netrc output");
		return NULL;
	}
	memset(&tWalk, 0, sizeof(tWalk));
	tWalk.Output = pOutput;
	tWalk.Context = &tContext;
	xvoTableWalk(pHosts, __xrtNetrcWriteHost, &tWalk);
	if ( !tWalk.Failed ) { xvoTableWalk(pMacros, __xrtNetrcWriteMacro, &tWalk); }
	if ( tWalk.Failed ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}

bool xrtNetrcWrite(xvalue pDocument, xwriter pWriter,
	bool bTakeOwnership, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrtnetrcerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && pWriter != NULL &&
		xrtWriterWriteAll(pWriter, pOutput->Buffer, pOutput->Length) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
			0u, 0u, 0u, "cannot write netrc stream");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
	return bResult;
}

bool xrtNetrcSave(str sPath, xvalue pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrtnetrcerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && sPath != NULL &&
		xrtFileWriteAllAtomic(sPath, (str)pOutput->Buffer, pOutput->Length,
			XRT_CP_BINARY);
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
			0u, 0u, 0u, "cannot save netrc file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
