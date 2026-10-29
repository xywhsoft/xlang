#include "xrt_netrc_internal.h"

static bool __xrtNetrcAppend(xbuffer* pOutput, const void* pData, size_t iSize,
	xrt_netrc_context* pContext)
{
	uint64 iLimit;
	if ( pOutput == NULL || pContext == NULL ) { return false; }
	iLimit = pContext->Options.tLimits.iMaxOutputBytes;
	if ( iLimit != 0u && ((uint64)pOutput->Size > iLimit ||
		(uint64)iSize > iLimit - pOutput->Size) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_LIMIT,
			0u, 0u, 0u, "netrc output exceeds the configured limit");
		return false;
	}
	if ( !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_MEMORY,
			0u, 0u, 0u, "cannot grow netrc output");
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
	/* 关键字作为普通值时必须加引号，保证输出能够原样读回。 */
	if ( (iSize == 7u && memcmp(sText, "machine", 7u) == 0) ||
		(iSize == 7u && memcmp(sText, "default", 7u) == 0) ||
		(iSize == 6u && memcmp(sText, "macdef", 6u) == 0) ) { return true; }
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c <= 0x20u || c == 0x7fu || c == '#' || c == '"' ||
			c == '\'' || c == '\\' ) { return true; }
	}
	return false;
}

static bool __xrtNetrcMacroLineBlank(const char* sText, size_t iSize)
{
	size_t i;
	for ( i = 0u; i < iSize; ++i ) {
		if ( sText[i] != ' ' && sText[i] != '\t' ) { return false; }
	}
	return true;
}

static bool __xrtNetrcAppendToken(xbuffer* pOutput, const char* sText,
	size_t iSize, xrt_netrc_context* pContext)
{
	size_t i;
	if ( sText == NULL || memchr(sText, '\0', iSize) != NULL ||
		memchr(sText, '\r', iSize) != NULL ||
		memchr(sText, '\n', iSize) != NULL ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "netrc token contains NUL or newline");
		return false;
	}
	if ( !xrtUtf8Valid((xstrview){ sText, iSize }, NULL) ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_UTF8,
			0u, 0u, 0u, "netrc token is not valid UTF-8");
		return false;
	}
	if ( pContext->Options.tLimits.iMaxItemBytes != 0u &&
		(uint64)iSize > pContext->Options.tLimits.iMaxItemBytes ) {
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
	return __xrtNetrcLimitNode(pContext, 0u, 0u, 0u) &&
		__xrtNetrcAppendText(pOutput, "  ", pContext) &&
		__xrtNetrcAppendText(pOutput, sKey, pContext) &&
		__xrtNetrcAppendText(pOutput, " ", pContext) &&
		__xrtNetrcAppendToken(pOutput, Value.Data, Value.Size, pContext) &&
		__xrtNetrcAppendText(pOutput, "\n", pContext);
}

static bool __xrtNetrcKnownAuthFields(xvalue* pAuth,
	xrt_netrc_context* pContext)
{
	size_t iKnown = (xrtValueObjectHas(pAuth, xrtStrView("login")) ? 1u : 0u) +
		(xrtValueObjectHas(pAuth, xrtStrView("account")) ? 1u : 0u) +
		(xrtValueObjectHas(pAuth, xrtStrView("password")) ? 1u : 0u);
	if ( xrtValueCount(pAuth) == iKnown ) { return true; }
	__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_TYPE,
		0u, 0u, 0u, "unsupported netrc authentication field");
	return false;
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
		!__xrtNetrcKnownAuthFields(pAuth, pWalk->Context) ||
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

static bool __xrtNetrcWriteMachine(xstrview Key, xvalue* pAuth,
	xrt_netrc_write_context* pWalk)
{
	/* default 最后单独写出，避免外部 netrc 读取器提前匹配所有主机。 */
	if ( Key.Size == 7u && memcmp(Key.Data, "default", 7u) == 0 ) { return true; }
	return __xrtNetrcWriteHost(Key, pAuth, pWalk);
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
		if ( memchr(Line.Data, '\0', Line.Size) != NULL ||
			memchr(Line.Data, '\r', Line.Size) != NULL ||
			memchr(Line.Data, '\n', Line.Size) != NULL ||
			__xrtNetrcMacroLineBlank(Line.Data, Line.Size) ) { goto fail; }
		if ( !xrtUtf8Valid(Line, NULL) ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_UTF8,
				0u, 0u, 0u, "netrc macro line is not valid UTF-8");
			goto fail;
		}
		if ( pWalk->Context->Options.tLimits.iMaxItemBytes != 0u &&
			(uint64)Line.Size > pWalk->Context->Options.tLimits.iMaxItemBytes ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_LIMIT,
				0u, 0u, 0u, "netrc macro line exceeds the configured item limit");
			goto fail;
		}
		if ( !__xrtNetrcAppend(pWalk->Output, Line.Data, Line.Size, pWalk->Context) ||
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
		__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
			0u, 0u, 0u, "netrc entries must be a dict");
		return false;
	}
	while ( (pValue = xrtValueIterNext(&Iterator, &Key)) != NULL ) {
		if ( Key.Type != XVALUE_KEY_STRING ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
				0u, 0u, 0u, "netrc entry names must be strings");
			xrtValueIterEnd(&Iterator);
			return false;
		}
		if ( Key.String.Size == 0u ) {
			__xrtNetrcSetError(pWalk->Context->Error, XRT_NETRC_ERROR_TYPE,
				0u, 0u, 0u, "netrc entry names must be non-empty");
			xrtValueIterEnd(&Iterator);
			return false;
		}
		if ( !pWriter(Key.String, pValue, pWalk) ) {
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
	xvalue* pDefault = NULL;
	xbuffer* pOutput = NULL;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtNetrcErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Error = pError;
	if ( !__xrtNetrcResolveOptions(pOptions, &tContext.Options, pError) ) {
		return NULL;
	}
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
	if ( xrtValueCount(pDocument) != 2u ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_TYPE, 0u, 0u, 0u,
			"unsupported netrc document field");
		return NULL;
	}
	pDefault = xrtValueObjectGet(pHosts, xrtStrView("default"));
	pOutput = xrtBufferCreate();
	if ( pOutput == NULL || !xrtBufferReserve(pOutput, 512u) ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate netrc output");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	memset(&tWalk, 0, sizeof(tWalk));
	tWalk.Output = pOutput;
	tWalk.Context = &tContext;
	if ( !__xrtNetrcWriteObject(pHosts, __xrtNetrcWriteMachine, &tWalk) ||
		(pDefault != NULL && !__xrtNetrcWriteHost(xrtStrView("default"),
			pDefault, &tWalk)) ||
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
	xbuffer* pOutput;
	bool bResult;
	if ( pWriter == NULL ) {
		xrtNetrcErrorInit(pResolvedError);
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_ARGUMENT,
			0u, 0u, 0u, "netrc writer is null");
		return false;
	}
	pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	if ( pOutput != NULL ) { xrtClearError(); }
	bResult = pOutput != NULL &&
		xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		if ( xrtErrorKind(xrtGetError()) == XERR_MEMORY ) {
			__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_MEMORY,
				0u, 0u, 0u, "cannot allocate netrc writer resources");
		} else {
			__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
				0u, 0u, 0u, "cannot write netrc stream");
		}
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership ) {
		xerror* pCause = bResult ? NULL : xrtTakeError();
		bool bClosed = xrtWriterDestroy(pWriter);
		if ( pCause != NULL ) { xrtSetErrorTake(pCause); }
		if ( !bClosed && bResult ) {
			const xerror* pCloseError = xrtGetError();
			int iCode = pCloseError != NULL && xrtErrorKind(pCloseError) == XERR_MEMORY ?
				XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO;
			/* 写入与刷新成功后，关闭失败仍表示持久化未完成。 */
			bResult = false;
			__xrtNetrcSetError(pResolvedError, iCode, 0u, 0u, 0u,
				iCode == XRT_NETRC_ERROR_MEMORY ?
				"cannot allocate netrc writer close resources" :
				"cannot close netrc stream");
		}
	}
	return bResult;
}

bool xrtNetrcSave(str sPath, xvalue* pDocument,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrtnetrcerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput;
	str sParent = NULL;
	str sTemporary = NULL;
	xfile File = NULL;
	bool bResult = false;
	if ( sPath == NULL || sPath[0] == '\0' ) {
		xrtNetrcErrorInit(pResolvedError);
		__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_ARGUMENT,
			0u, 0u, 0u, "netrc path is empty");
		return false;
	}
	pOutput = xrtNetrcStringify(pDocument, pOptions, pResolvedError);
	if ( pOutput != NULL ) {
		xrtClearError();
		sParent = xrtPathParent(sPath);
		if ( sParent != NULL ) {
			/* 凭据文件使用同目录 0600 临时文件，写完整后再替换目标。 */
			File = xrtFileTemp(sParent[0] != 0 ? sParent : ".",
				".xrt-netrc-", ".tmp", &sTemporary);
		}
		if ( File != NULL ) {
			bResult = xrtWriteFull(File, pOutput->Data, pOutput->Size, NULL) &&
				xrtFlush(File);
			xerror* pCause = bResult ? NULL : xrtTakeError();
			if ( !xrtClose(File) ) { bResult = false; }
			if ( pCause != NULL ) { xrtSetErrorTake(pCause); }
			File = NULL;
			if ( bResult ) { bResult = xrtPathRename(sTemporary, sPath, true); }
		}
	}
	if ( !bResult && pResolvedError->iCode == XRT_NETRC_OK ) {
		const xerror* pFailure = xrtGetError();
		if ( pFailure != NULL && xrtErrorKind(pFailure) == XERR_MEMORY ) {
			__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_MEMORY,
				0u, 0u, 0u, "cannot allocate netrc save resources");
		} else {
			__xrtNetrcSetError(pResolvedError, XRT_NETRC_ERROR_IO,
				0u, 0u, 0u, "cannot save netrc file");
		}
	}
	if ( !bResult && sTemporary != NULL ) {
		xerror* pCause = xrtTakeError();
		xrtFileDelete(sTemporary);
		if ( pCause != NULL ) { xrtSetErrorTake(pCause); }
	}
	xrtFree(sTemporary);
	xrtFree(sParent);
	xrtBufferDestroy(pOutput);
	return bResult;
}
