#include "xrt_netrc_internal.h"

typedef struct {
	const char* pData;
	size_t iSize;
	size_t iPos;
	uint64 iLine;
	uint64 iColumn;
	xrt_netrc_context* Context;
} xrt_netrc_scanner;

typedef struct {
	str sText;
	size_t iSize;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
} xrt_netrc_token;

typedef struct {
	xrt_netrc_scanner Scanner;
	xrt_netrc_token Push;
	bool bHasPush;
} xrt_netrc_parser;

static xrtnetrcoptions __xrtNetrcOptions(const xrtnetrcoptions* pOptions)
{
	xrtnetrcoptions tResult;
	xrtNetrcOptionsInit(&tResult);
	if ( pOptions != NULL && pOptions->iSize >= sizeof(*pOptions) &&
		pOptions->iVersion == XRT_NETRC_OPTIONS_VERSION ) { tResult = *pOptions; }
	return tResult;
}

static void __xrtNetrcTokenUnit(xrt_netrc_token* pToken)
{
	if ( pToken == NULL ) { return; }
	xrtFree(pToken->sText);
	memset(pToken, 0, sizeof(*pToken));
}

static void __xrtNetrcAdvance(xrt_netrc_scanner* pScanner)
{
	char c;
	if ( pScanner == NULL || pScanner->iPos >= pScanner->iSize ) { return; }
	c = pScanner->pData[pScanner->iPos++];
	if ( c == '\n' ) {
		pScanner->iLine++;
		pScanner->iColumn = 1u;
	} else {
		pScanner->iColumn++;
	}
}

static bool __xrtNetrcAppendChar(xbuffer pBuffer, char c,
	xrt_netrc_scanner* pScanner)
{
	uint64 iLimit = pScanner->Context->Options.tLimits.iMaxItemBytes;
	if ( (iLimit != 0u && pBuffer->Length >= iLimit) ||
		pBuffer->Length >= UINT32_MAX ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_LIMIT,
			pScanner->iLine, pScanner->iColumn, pScanner->iPos,
			"netrc token exceeds the configured item limit");
		return FALSE;
	}
	return xrtBufferAppend(pBuffer, &c, 1u, XBUF_BINARY);
}

static void __xrtNetrcSkipSpace(xrt_netrc_scanner* pScanner)
{
	for ( ;; ) {
		while ( pScanner->iPos < pScanner->iSize ) {
			char c = pScanner->pData[pScanner->iPos];
			if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' ) { break; }
			__xrtNetrcAdvance(pScanner);
		}
		if ( pScanner->iPos >= pScanner->iSize ||
			pScanner->pData[pScanner->iPos] != '#' ) { return; }
		while ( pScanner->iPos < pScanner->iSize &&
			pScanner->pData[pScanner->iPos] != '\n' ) {
			__xrtNetrcAdvance(pScanner);
		}
	}
}

static bool __xrtNetrcScanToken(xrt_netrc_scanner* pScanner,
	xrt_netrc_token* pToken, bool* pEof)
{
	xbuffer pBuffer = NULL;
	char cQuote = 0;
	bool bClosed = FALSE;
	if ( pEof ) { *pEof = FALSE; }
	if ( pScanner == NULL || pToken == NULL ) { return FALSE; }
	memset(pToken, 0, sizeof(*pToken));
	__xrtNetrcSkipSpace(pScanner);
	if ( pScanner->iPos >= pScanner->iSize ) {
		if ( pEof ) { *pEof = TRUE; }
		return TRUE;
	}
	pToken->iLine = pScanner->iLine;
	pToken->iColumn = pScanner->iColumn;
	pToken->iOffset = pScanner->iPos;
	pBuffer = xrtBufferCreate(32u);
	if ( pBuffer == NULL ) { goto memory_error; }
	if ( pScanner->pData[pScanner->iPos] == '\'' ||
		pScanner->pData[pScanner->iPos] == '"' ) {
		cQuote = pScanner->pData[pScanner->iPos];
		__xrtNetrcAdvance(pScanner);
	}
	while ( pScanner->iPos < pScanner->iSize ) {
		char c = pScanner->pData[pScanner->iPos];
		if ( cQuote != 0 && c == cQuote ) {
			__xrtNetrcAdvance(pScanner);
			bClosed = TRUE;
			break;
		}
		if ( cQuote == 0 && (c == ' ' || c == '\t' || c == '\r' ||
			c == '\n' || c == '#') ) { break; }
		if ( c == '\\' ) {
			__xrtNetrcAdvance(pScanner);
			if ( pScanner->iPos >= pScanner->iSize ) {
				__xrtNetrcSetError(pScanner->Context->Error,
					XRT_NETRC_ERROR_SYNTAX, pToken->iLine, pToken->iColumn,
					pToken->iOffset, "netrc token ends with an escape character");
				goto fail;
			}
			c = pScanner->pData[pScanner->iPos];
		}
		if ( !__xrtNetrcAppendChar(pBuffer, c, pScanner) ) { goto fail; }
		__xrtNetrcAdvance(pScanner);
	}
	if ( cQuote != 0 && !bClosed ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_SYNTAX,
			pToken->iLine, pToken->iColumn, pToken->iOffset,
			"unterminated quoted netrc token");
		goto fail;
	}
	if ( pBuffer->Length > 0u ) {
		pToken->sText = xrtCopyStr((str)pBuffer->Buffer, pBuffer->Length);
	} else {
		pToken->sText = xrtCopyStr((str)"", 0u);
	}
	pToken->iSize = pBuffer->Length;
	xrtBufferDestroy(pBuffer);
	return pToken->sText != NULL;

memory_error:
	__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_MEMORY,
		pScanner->iLine, pScanner->iColumn, pScanner->iPos,
		"cannot allocate netrc lexer state");
fail:
	xrtBufferDestroy(pBuffer);
	return FALSE;
}

static bool __xrtNetrcNext(xrt_netrc_parser* pParser,
	xrt_netrc_token* pToken, bool* pEof)
{
	if ( pParser->bHasPush ) {
		*pToken = pParser->Push;
		memset(&pParser->Push, 0, sizeof(pParser->Push));
		pParser->bHasPush = FALSE;
		if ( pEof ) { *pEof = FALSE; }
		return TRUE;
	}
	return __xrtNetrcScanToken(&pParser->Scanner, pToken, pEof);
}

static bool __xrtNetrcPush(xrt_netrc_parser* pParser, xrt_netrc_token* pToken)
{
	if ( pParser->bHasPush ) { return FALSE; }
	pParser->Push = *pToken;
	memset(pToken, 0, sizeof(*pToken));
	pParser->bHasPush = TRUE;
	return TRUE;
}

static bool __xrtNetrcTokenEquals(const xrt_netrc_token* pToken,
	const char* sText)
{
	return pToken != NULL && pToken->sText != NULL &&
		strcmp((const char*)pToken->sText, sText) == 0;
}

static bool __xrtNetrcIsTopLevel(const xrt_netrc_token* pToken)
{
	return __xrtNetrcTokenEquals(pToken, "machine") ||
		__xrtNetrcTokenEquals(pToken, "default") ||
		__xrtNetrcTokenEquals(pToken, "macdef");
}

static bool __xrtNetrcSetAuthField(xvalue pAuth, const char* sField,
	const xrt_netrc_token* pValue)
{
	xvalue pText = __xrtNetrcText((const char*)pValue->sText, pValue->iSize);
	return pText != NULL && __xrtNetrcSetValue(pAuth, sField, pText);
}

static xvalue __xrtNetrcParseAuth(xrt_netrc_parser* pParser,
	const xrt_netrc_token* pStart)
{
	xvalue pAuth = xvoCreateTable();
	bool bLogin = FALSE, bAccount = FALSE, bPassword = FALSE;
	if ( pAuth == NULL ||
		!__xrtNetrcSetValue(pAuth, "login", xvoCreateNull()) ||
		!__xrtNetrcSetValue(pAuth, "account", xvoCreateNull()) ||
		!__xrtNetrcSetValue(pAuth, "password", xvoCreateNull()) ) {
		xvoUnref(pAuth);
		return NULL;
	}
	for ( ;; ) {
		xrt_netrc_token tField, tValue;
		bool bEof = FALSE;
		const char* sCanonical = NULL;
		bool* pSeen = NULL;
		memset(&tField, 0, sizeof(tField));
		memset(&tValue, 0, sizeof(tValue));
		if ( !__xrtNetrcNext(pParser, &tField, &bEof) ) { goto fail; }
		if ( bEof ) { return pAuth; }
		if ( __xrtNetrcIsTopLevel(&tField) ) {
			if ( !__xrtNetrcPush(pParser, &tField) ) { __xrtNetrcTokenUnit(&tField); goto fail; }
			return pAuth;
		}
		if ( __xrtNetrcTokenEquals(&tField, "login") ||
			__xrtNetrcTokenEquals(&tField, "user") ) {
			sCanonical = "login"; pSeen = &bLogin;
		} else if ( __xrtNetrcTokenEquals(&tField, "account") ) {
			sCanonical = "account"; pSeen = &bAccount;
		} else if ( __xrtNetrcTokenEquals(&tField, "password") ||
			__xrtNetrcTokenEquals(&tField, "passwd") ) {
			sCanonical = "password"; pSeen = &bPassword;
		} else {
			__xrtNetrcSetError(pParser->Scanner.Context->Error,
				XRT_NETRC_ERROR_SYNTAX, tField.iLine, tField.iColumn,
				tField.iOffset, "unknown netrc machine field");
			__xrtNetrcTokenUnit(&tField);
			goto fail;
		}
		if (*pSeen && pParser->Scanner.Context->Options.bStrictDuplicates ) {
			__xrtNetrcSetError(pParser->Scanner.Context->Error,
				XRT_NETRC_ERROR_DUPLICATE, tField.iLine, tField.iColumn,
				tField.iOffset, "duplicate netrc machine field");
			__xrtNetrcTokenUnit(&tField);
			goto fail;
		}
		if ( !__xrtNetrcNext(pParser, &tValue, &bEof) || bEof ||
			__xrtNetrcIsTopLevel(&tValue) ) {
			__xrtNetrcSetError(pParser->Scanner.Context->Error,
				XRT_NETRC_ERROR_SYNTAX, tField.iLine, tField.iColumn,
				tField.iOffset, "netrc machine field has no value");
			__xrtNetrcTokenUnit(&tField);
			__xrtNetrcTokenUnit(&tValue);
			goto fail;
		}
		if ( !__xrtNetrcLimitNode(pParser->Scanner.Context, tValue.iLine,
			tValue.iColumn, tValue.iOffset) ||
			!__xrtNetrcSetAuthField(pAuth, sCanonical, &tValue) ) {
			__xrtNetrcTokenUnit(&tField);
			__xrtNetrcTokenUnit(&tValue);
			goto fail;
		}
		*pSeen = TRUE;
		__xrtNetrcTokenUnit(&tField);
		__xrtNetrcTokenUnit(&tValue);
	}

fail:
	if ( pParser->Scanner.Context->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pParser->Scanner.Context->Error,
			XRT_NETRC_ERROR_MEMORY, pStart->iLine, pStart->iColumn,
			pStart->iOffset, "cannot allocate netrc machine entry");
	}
	xvoUnref(pAuth);
	return NULL;
}

static bool __xrtNetrcLineBlank(const char* pData, size_t iSize)
{
	size_t i;
	for ( i = 0u; i < iSize; ++i ) {
		if ( pData[i] != ' ' && pData[i] != '\t' && pData[i] != '\r' ) { return FALSE; }
	}
	return TRUE;
}

static xvalue __xrtNetrcParseMacro(xrt_netrc_parser* pParser,
	const xrt_netrc_token* pName)
{
	xrt_netrc_scanner* pScanner = &pParser->Scanner;
	xvalue pLines = xvoCreateArray();
	if ( pLines == NULL ) { return NULL; }
	while ( pScanner->iPos < pScanner->iSize &&
		pScanner->pData[pScanner->iPos] != '\n' ) { __xrtNetrcAdvance(pScanner); }
	if ( pScanner->iPos < pScanner->iSize ) { __xrtNetrcAdvance(pScanner); }
	while ( pScanner->iPos < pScanner->iSize ) {
		size_t iStart = pScanner->iPos;
		uint64 iLine = pScanner->iLine;
		while ( pScanner->iPos < pScanner->iSize &&
			pScanner->pData[pScanner->iPos] != '\n' ) { __xrtNetrcAdvance(pScanner); }
		{
			size_t iEnd = pScanner->iPos;
			if ( iEnd > iStart && pScanner->pData[iEnd - 1u] == '\r' ) { iEnd--; }
			if ( __xrtNetrcLineBlank(pScanner->pData + iStart, iEnd - iStart) ) {
				if ( pScanner->iPos < pScanner->iSize ) { __xrtNetrcAdvance(pScanner); }
				return pLines;
			}
			if ( (pScanner->Context->Options.tLimits.iMaxItemBytes != 0u &&
				(uint64)(iEnd - iStart) > pScanner->Context->Options.tLimits.iMaxItemBytes) ||
				!__xrtNetrcLimitEntry(pScanner->Context, iLine, 1u, iStart) ||
				!__xrtNetrcLimitNode(pScanner->Context, iLine, 1u, iStart) ) { goto fail; }
			{
				xvalue pText = __xrtNetrcText(pScanner->pData + iStart, iEnd - iStart);
				if ( pText == NULL || !xvoArrayAppendValue(pLines, pText, TRUE) ) {
					if ( pText != NULL ) { xvoUnref(pText); }
					goto fail;
				}
			}
		}
		if ( pScanner->iPos < pScanner->iSize ) { __xrtNetrcAdvance(pScanner); }
	}
	return pLines;

fail:
	if ( pScanner->Context->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_MEMORY,
			pName->iLine, pName->iColumn, pName->iOffset,
			"cannot allocate netrc macro");
	}
	xvoUnref(pLines);
	return NULL;
}

static xvalue __xrtNetrcParseDocument(const char* pData, size_t iSize,
	xrt_netrc_context* pContext)
{
	xrt_netrc_parser tParser;
	xvalue pRoot = NULL, pHosts = NULL, pMacros = NULL;
	memset(&tParser, 0, sizeof(tParser));
	tParser.Scanner.pData = pData;
	tParser.Scanner.iSize = iSize;
	tParser.Scanner.iLine = 1u;
	tParser.Scanner.iColumn = 1u;
	tParser.Scanner.Context = pContext;
	pRoot = xvoCreateTable();
	pHosts = xvoCreateTable();
	pMacros = xvoCreateTable();
	if ( pRoot == NULL || pHosts == NULL || pMacros == NULL ||
		!__xrtNetrcSetValue(pRoot, "hosts", pHosts) ||
		!__xrtNetrcSetValue(pRoot, "macros", pMacros) ) { goto fail; }
	for ( ;; ) {
		xrt_netrc_token tKeyword, tName;
		bool bEof = FALSE;
		memset(&tKeyword, 0, sizeof(tKeyword));
		memset(&tName, 0, sizeof(tName));
		if ( !__xrtNetrcNext(&tParser, &tKeyword, &bEof) ) { goto fail; }
		if ( bEof ) { break; }
		if ( __xrtNetrcTokenEquals(&tKeyword, "machine") ||
			__xrtNetrcTokenEquals(&tKeyword, "default") ) {
			const char* sName = "default";
			size_t iNameSize = 7u;
			xvalue pAuth;
			if ( __xrtNetrcTokenEquals(&tKeyword, "machine") ) {
				if ( !__xrtNetrcNext(&tParser, &tName, &bEof) || bEof ||
					__xrtNetrcIsTopLevel(&tName) || tName.iSize == 0u ) {
					__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_SYNTAX,
						tKeyword.iLine, tKeyword.iColumn, tKeyword.iOffset,
						"machine requires a non-empty name");
					__xrtNetrcTokenUnit(&tKeyword);
					__xrtNetrcTokenUnit(&tName);
					goto fail;
				}
				sName = (const char*)tName.sText;
				iNameSize = tName.iSize;
			}
			if ( xvoTableExists(pHosts, (str)sName, (uint32)iNameSize) &&
				pContext->Options.bStrictDuplicates ) {
				__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_DUPLICATE,
					tKeyword.iLine, tKeyword.iColumn, tKeyword.iOffset,
					"duplicate netrc machine entry");
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			if ( !__xrtNetrcLimitEntry(pContext, tKeyword.iLine,
				tKeyword.iColumn, tKeyword.iOffset) ||
				!__xrtNetrcLimitNode(pContext, tKeyword.iLine,
				tKeyword.iColumn, tKeyword.iOffset) ) {
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			pAuth = __xrtNetrcParseAuth(&tParser, &tKeyword);
			if ( pAuth == NULL || !xvoTableSetValue(pHosts, sName,
				(uint32)iNameSize, pAuth, TRUE) ) {
				if ( pAuth != NULL ) { xvoUnref(pAuth); }
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			__xrtNetrcTokenUnit(&tName);
		} else if ( __xrtNetrcTokenEquals(&tKeyword, "macdef") ) {
			xvalue pLines;
			if ( !__xrtNetrcNext(&tParser, &tName, &bEof) || bEof ||
				tName.iSize == 0u || __xrtNetrcIsTopLevel(&tName) ) {
				__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_SYNTAX,
					tKeyword.iLine, tKeyword.iColumn, tKeyword.iOffset,
					"macdef requires a non-empty name");
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			if ( xvoTableExists(pMacros, tName.sText, (uint32)tName.iSize) &&
				pContext->Options.bStrictDuplicates ) {
				__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_DUPLICATE,
					tName.iLine, tName.iColumn, tName.iOffset,
					"duplicate netrc macro");
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			if ( !__xrtNetrcLimitEntry(pContext, tName.iLine, tName.iColumn,
				tName.iOffset) || !__xrtNetrcLimitNode(pContext, tName.iLine,
				tName.iColumn, tName.iOffset) ) {
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			pLines = __xrtNetrcParseMacro(&tParser, &tName);
			if ( pLines == NULL || !xvoTableSetValue(pMacros, tName.sText,
				(uint32)tName.iSize, pLines, TRUE) ) {
				if ( pLines != NULL ) { xvoUnref(pLines); }
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			__xrtNetrcTokenUnit(&tName);
		} else {
			__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_SYNTAX,
				tKeyword.iLine, tKeyword.iColumn, tKeyword.iOffset,
				"expected machine, default, or macdef");
			__xrtNetrcTokenUnit(&tKeyword);
			goto fail;
		}
		__xrtNetrcTokenUnit(&tKeyword);
	}
	__xrtNetrcTokenUnit(&tParser.Push);
	return pRoot;

fail:
	__xrtNetrcTokenUnit(&tParser.Push);
	if ( pContext->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_MEMORY,
			0u, 0u, 0u, "cannot allocate netrc document");
	}
	xvoUnref(pRoot);
	return NULL;
}

xvalue xrtNetrcParse(xreader pReader, bool bTakeOwnership,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrt_netrc_context tContext;
	xbuffer pInput = NULL;
	xvalue pResult = NULL;
	uint64 iSize = 0u;
	uint64 iLimit;
	const char* pData;
	size_t iDataSize;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtNetrcErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Options = __xrtNetrcOptions(pOptions);
	tContext.Error = pError;
	if ( pReader == NULL ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid netrc reader");
		goto cleanup;
	}
	iLimit = tContext.Options.tLimits.iMaxInputBytes;
	if ( xrtReaderSize(pReader, &iSize) && iLimit != 0u && iSize > iLimit ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_LIMIT, 0u, 0u, 0u,
			"netrc input exceeds the configured limit");
		goto cleanup;
	}
	pInput = xrtReaderReadAll(pReader, iLimit);
	if ( pInput == NULL ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_IO, 0u, 0u, 0u,
			"cannot read netrc stream or input limit was exceeded");
		goto cleanup;
	}
	pData = (const char*)pInput->Buffer;
	iDataSize = pInput->Length;
	if ( iDataSize >= 3u && (unsigned char)pData[0] == 0xefu &&
		(unsigned char)pData[1] == 0xbbu && (unsigned char)pData[2] == 0xbfu ) {
		pData += 3;
		iDataSize -= 3u;
	}
	if ( !xrtIsUTF8((str)pData, iDataSize) ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_UTF8, 1u, 1u, 0u,
			"netrc input is not valid UTF-8");
		goto cleanup;
	}
	pResult = __xrtNetrcParseDocument(pData, iDataSize, &tContext);

cleanup:
	xrtBufferDestroy(pInput);
	if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
	return pResult;
}

xvalue xrtNetrcParseMemory(const void* pData, uint64 iSize,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xreader pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory(pData, iSize) : NULL;
	return xrtNetrcParse(pReader, TRUE, pOptions, pError);
}

xvalue xrtNetrcLoad(str sPath, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError)
{
	xreader pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtNetrcParse(pReader, TRUE, pOptions, pError);
}
