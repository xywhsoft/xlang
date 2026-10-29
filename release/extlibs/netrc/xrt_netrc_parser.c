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
	bool bQuoted;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
} xrt_netrc_token;

typedef struct {
	xrt_netrc_scanner Scanner;
	xrt_netrc_token Push;
	bool bHasPush;
} xrt_netrc_parser;

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
	/* CRLF 由 LF 计行；已接受的单独 CR 分隔符也应计行。 */
	if ( c == '\n' || (c == '\r' && (pScanner->iPos >= pScanner->iSize ||
		pScanner->pData[pScanner->iPos] != '\n')) ) {
		pScanner->iLine++;
		pScanner->iColumn = 1u;
	} else {
		pScanner->iColumn++;
	}
}

static bool __xrtNetrcAppendChar(xbuffer* pBuffer, char c,
	xrt_netrc_scanner* pScanner)
{
	uint64 iLimit = pScanner->Context->Options.tLimits.iMaxItemBytes;
	if ( iLimit != 0u && pBuffer->Size >= iLimit ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_LIMIT,
			pScanner->iLine, pScanner->iColumn, pScanner->iPos,
			"netrc token exceeds the configured item limit");
		return false;
	}
	if ( !xrtBufferAppendByte(pBuffer, (uint8)c) ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_MEMORY,
			pScanner->iLine, pScanner->iColumn, pScanner->iPos,
			"cannot grow netrc token");
		return false;
	}
	return true;
}

static bool __xrtNetrcSkipSpace(xrt_netrc_scanner* pScanner)
{
	for ( ;; ) {
		while ( pScanner->iPos < pScanner->iSize ) {
			char c = pScanner->pData[pScanner->iPos];
			if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' ) { break; }
			__xrtNetrcAdvance(pScanner);
		}
		if ( pScanner->iPos >= pScanner->iSize ||
			pScanner->pData[pScanner->iPos] != '#' ) { return true; }
		/* 注释也在单独 CR 处结束，不吞掉下一行的凭据。 */
		while ( pScanner->iPos < pScanner->iSize &&
			pScanner->pData[pScanner->iPos] != '\n' &&
			pScanner->pData[pScanner->iPos] != '\r' ) {
			if ( pScanner->pData[pScanner->iPos] == '\0' ) {
				__xrtNetrcSetError(pScanner->Context->Error,
					XRT_NETRC_ERROR_SYNTAX, pScanner->iLine,
					pScanner->iColumn, pScanner->iPos,
					"NUL in netrc comment");
				return false;
			}
			__xrtNetrcAdvance(pScanner);
		}
	}
}

static bool __xrtNetrcScanToken(xrt_netrc_scanner* pScanner,
	xrt_netrc_token* pToken, bool* pEof)
{
	xbuffer* pBuffer = NULL;
	char cQuote = 0;
	bool bClosed = false;
	if ( pEof ) { *pEof = false; }
	if ( pScanner == NULL || pToken == NULL ) { return false; }
	memset(pToken, 0, sizeof(*pToken));
	if ( !__xrtNetrcSkipSpace(pScanner) ) { return false; }
	if ( pScanner->iPos >= pScanner->iSize ) {
		if ( pEof ) { *pEof = true; }
		return true;
	}
	pToken->iLine = pScanner->iLine;
	pToken->iColumn = pScanner->iColumn;
	pToken->iOffset = pScanner->iPos;
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL || !xrtBufferReserve(pBuffer, 32u) ) { goto memory_error; }
	if ( pScanner->pData[pScanner->iPos] == '\'' ||
		pScanner->pData[pScanner->iPos] == '"' ) {
		cQuote = pScanner->pData[pScanner->iPos];
		pToken->bQuoted = true;
		__xrtNetrcAdvance(pScanner);
	}
	while ( pScanner->iPos < pScanner->iSize ) {
		char c = pScanner->pData[pScanner->iPos];
		if ( cQuote != 0 && c == cQuote ) {
			__xrtNetrcAdvance(pScanner);
			bClosed = true;
			break;
		}
		if ( cQuote == 0 && (c == ' ' || c == '\t' || c == '\r' ||
			c == '\n' || c == '#') ) { break; }
		if ( c == '\0' || (cQuote != 0 && (c == '\r' || c == '\n')) ) {
			__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_SYNTAX,
				pScanner->iLine, pScanner->iColumn, pScanner->iPos,
				"NUL or newline in netrc token");
			goto fail;
		}
		if ( c == '\\' ) {
			__xrtNetrcAdvance(pScanner);
			if ( pScanner->iPos >= pScanner->iSize ) {
				__xrtNetrcSetError(pScanner->Context->Error,
					XRT_NETRC_ERROR_SYNTAX, pToken->iLine, pToken->iColumn,
					pToken->iOffset, "netrc token ends with an escape character");
				goto fail;
			}
			c = pScanner->pData[pScanner->iPos];
			if ( c == '\0' || c == '\r' || c == '\n' ) {
				__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_SYNTAX,
					pScanner->iLine, pScanner->iColumn, pScanner->iPos,
					"NUL or newline in netrc escape");
				goto fail;
			}
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
	if ( cQuote != 0 && pScanner->iPos < pScanner->iSize ) {
		char c = pScanner->pData[pScanner->iPos];
		if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '#' ) {
			__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_SYNTAX,
				pScanner->iLine, pScanner->iColumn, pScanner->iPos,
				"netrc quoted token requires a separator");
			goto fail;
		}
	}
	pToken->iSize = pBuffer->Size;
	if ( !xrtBufferAppendByte(pBuffer, 0u) ) { goto memory_error; }
	pToken->sText = (str)xrtBufferTake(pBuffer, NULL, NULL);
	xrtBufferDestroy(pBuffer);
	if ( pToken->sText == NULL ) {
		__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_MEMORY,
			pToken->iLine, pToken->iColumn, pToken->iOffset,
			"cannot allocate netrc token");
		return false;
	}
	return true;

memory_error:
	__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_MEMORY,
		pScanner->iLine, pScanner->iColumn, pScanner->iPos,
		"cannot allocate netrc lexer state");
fail:
	xrtBufferDestroy(pBuffer);
	return false;
}

static bool __xrtNetrcNext(xrt_netrc_parser* pParser,
	xrt_netrc_token* pToken, bool* pEof)
{
	if ( pParser->bHasPush ) {
		*pToken = pParser->Push;
		memset(&pParser->Push, 0, sizeof(pParser->Push));
		pParser->bHasPush = false;
		if ( pEof ) { *pEof = false; }
		return true;
	}
	return __xrtNetrcScanToken(&pParser->Scanner, pToken, pEof);
}

static bool __xrtNetrcPush(xrt_netrc_parser* pParser, xrt_netrc_token* pToken)
{
	if ( pParser->bHasPush ) { return false; }
	pParser->Push = *pToken;
	memset(pToken, 0, sizeof(*pToken));
	pParser->bHasPush = true;
	return true;
}

static bool __xrtNetrcTokenEquals(const xrt_netrc_token* pToken,
	const char* sText)
{
	return pToken != NULL && pToken->sText != NULL &&
		pToken->iSize == strlen(sText) &&
		memcmp(pToken->sText, sText, pToken->iSize) == 0;
}

static bool __xrtNetrcIsTopLevel(const xrt_netrc_token* pToken)
{
	return pToken != NULL && !pToken->bQuoted &&
		(__xrtNetrcTokenEquals(pToken, "machine") ||
		__xrtNetrcTokenEquals(pToken, "default") ||
		__xrtNetrcTokenEquals(pToken, "macdef"));
}

static bool __xrtNetrcSetAuthField(xvalue* pAuth, const char* sField,
	xrt_netrc_token* pValue)
{
	xvalue* pText = xrtValueStringTake(&pValue->sText, pValue->iSize);
	return pText != NULL && __xrtNetrcSetValue(pAuth, sField, pText);
}

static xvalue* __xrtNetrcParseAuth(xrt_netrc_parser* pParser,
	const xrt_netrc_token* pStart)
{
	xvalue* pAuth = xrtValueObject();
	bool bLogin = false, bAccount = false, bPassword = false;
	if ( pAuth == NULL ||
		!__xrtNetrcSetValue(pAuth, "login", xrtValueNull()) ||
		!__xrtNetrcSetValue(pAuth, "account", xrtValueNull()) ||
		!__xrtNetrcSetValue(pAuth, "password", xrtValueNull()) ) {
		xrtValueRelease(pAuth);
		return NULL;
	}
	for ( ;; ) {
		xrt_netrc_token tField, tValue;
		bool bEof = false;
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
		*pSeen = true;
		__xrtNetrcTokenUnit(&tField);
		__xrtNetrcTokenUnit(&tValue);
	}

fail:
	if ( pParser->Scanner.Context->Error->iCode == XRT_NETRC_OK ) {
		__xrtNetrcSetError(pParser->Scanner.Context->Error,
			XRT_NETRC_ERROR_MEMORY, pStart->iLine, pStart->iColumn,
			pStart->iOffset, "cannot allocate netrc machine entry");
	}
	xrtValueRelease(pAuth);
	return NULL;
}

static bool __xrtNetrcLineBlank(const char* pData, size_t iSize)
{
	size_t i;
	for ( i = 0u; i < iSize; ++i ) {
		if ( pData[i] != ' ' && pData[i] != '\t' && pData[i] != '\r' ) { return false; }
	}
	return true;
}

static xvalue* __xrtNetrcParseMacro(xrt_netrc_parser* pParser,
	const xrt_netrc_token* pName)
{
	xrt_netrc_scanner* pScanner = &pParser->Scanner;
	xvalue* pLines = xrtValueArray();
	if ( pLines == NULL ) { return NULL; }
	while ( pScanner->iPos < pScanner->iSize &&
		pScanner->pData[pScanner->iPos] != '\n' ) {
		char c = pScanner->pData[pScanner->iPos];
		if ( c == '#' ) {
			while ( pScanner->iPos < pScanner->iSize &&
				pScanner->pData[pScanner->iPos] != '\n' ) {
				if ( pScanner->pData[pScanner->iPos] == '\0' ) {
					__xrtNetrcSetError(pScanner->Context->Error,
						XRT_NETRC_ERROR_SYNTAX, pScanner->iLine,
						pScanner->iColumn, pScanner->iPos,
						"NUL in netrc macro comment");
					goto fail;
				}
				__xrtNetrcAdvance(pScanner);
			}
			break;
		}
		if ( c != ' ' && c != '\t' && c != '\r' ) {
			__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_SYNTAX,
				pScanner->iLine, pScanner->iColumn, pScanner->iPos,
				"unexpected content after macdef name");
			goto fail;
		}
		__xrtNetrcAdvance(pScanner);
	}
	if ( pScanner->iPos < pScanner->iSize ) { __xrtNetrcAdvance(pScanner); }
	while ( pScanner->iPos < pScanner->iSize ) {
		size_t iStart = pScanner->iPos;
		uint64 iLine = pScanner->iLine;
		while ( pScanner->iPos < pScanner->iSize &&
			pScanner->pData[pScanner->iPos] != '\n' ) { __xrtNetrcAdvance(pScanner); }
		{
			size_t iEnd = pScanner->iPos;
			const char* pCR;
			if ( iEnd > iStart && pScanner->pData[iEnd - 1u] == '\r' ) { iEnd--; }
			pCR = memchr(pScanner->pData + iStart, '\r', iEnd - iStart);
			if ( pCR != NULL ) {
				size_t iOffset = (size_t)(pCR - pScanner->pData);
				__xrtNetrcSetError(pScanner->Context->Error,
					XRT_NETRC_ERROR_SYNTAX, iLine,
					(uint64)(iOffset - iStart + 1u), (uint64)iOffset,
					"CR inside netrc macro line");
				goto fail;
			}
			if ( memchr(pScanner->pData + iStart, '\0', iEnd - iStart) != NULL ) {
				__xrtNetrcSetError(pScanner->Context->Error,
					XRT_NETRC_ERROR_SYNTAX, iLine, 1u, iStart,
					"NUL in netrc macro");
				goto fail;
			}
			if ( __xrtNetrcLineBlank(pScanner->pData + iStart, iEnd - iStart) ) {
				if ( pScanner->iPos < pScanner->iSize ) { __xrtNetrcAdvance(pScanner); }
				return pLines;
			}
			if ( pScanner->Context->Options.tLimits.iMaxItemBytes != 0u &&
				(uint64)(iEnd - iStart) > pScanner->Context->Options.tLimits.iMaxItemBytes ) {
				__xrtNetrcSetError(pScanner->Context->Error, XRT_NETRC_ERROR_LIMIT,
					iLine, 1u, iStart, "netrc macro line exceeds the configured item limit");
				goto fail;
			}
			if ( !__xrtNetrcLimitEntry(pScanner->Context, iLine, 1u, iStart) ||
				!__xrtNetrcLimitNode(pScanner->Context, iLine, 1u, iStart) ) { goto fail; }
			{
				xvalue* pText = __xrtNetrcText(pScanner->pData + iStart, iEnd - iStart);
				if ( pText == NULL || !xrtValueArrayAppendTake(pLines, &pText) ) {
					xrtValueRelease(pText);
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
	xrtValueRelease(pLines);
	return NULL;
}

static xvalue* __xrtNetrcParseDocument(const char* pData, size_t iSize,
	xrt_netrc_context* pContext)
{
	xrt_netrc_parser tParser;
	xvalue* pRoot = NULL;
	xvalue* pHosts = NULL;
	xvalue* pMacros = NULL;
	memset(&tParser, 0, sizeof(tParser));
	tParser.Scanner.pData = pData;
	tParser.Scanner.iSize = iSize;
	tParser.Scanner.iLine = 1u;
	tParser.Scanner.iColumn = 1u;
	tParser.Scanner.Context = pContext;
	pRoot = xrtValueObject();
	if ( pRoot == NULL ) { goto fail; }
	pHosts = xrtValueObject();
	if ( pHosts == NULL ) { goto fail; }
	/* 成功后字典归根对象持有；失败时 SetValue 已释放待移交值。 */
	if ( !__xrtNetrcSetValue(pRoot, "hosts", pHosts) ) { goto fail; }
	pMacros = xrtValueObject();
	if ( pMacros == NULL ) { goto fail; }
	if ( !__xrtNetrcSetValue(pRoot, "macros", pMacros) ) { goto fail; }
	for ( ;; ) {
		xrt_netrc_token tKeyword, tName;
		bool bEof = false;
		memset(&tKeyword, 0, sizeof(tKeyword));
		memset(&tName, 0, sizeof(tName));
		if ( !__xrtNetrcNext(&tParser, &tKeyword, &bEof) ) { goto fail; }
		if ( bEof ) { break; }
		if ( !tKeyword.bQuoted &&
			(__xrtNetrcTokenEquals(&tKeyword, "machine") ||
			__xrtNetrcTokenEquals(&tKeyword, "default")) ) {
			const char* sName = "default";
			size_t iNameSize = 7u;
			xvalue* pAuth;
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
				if ( iNameSize == 7u && memcmp(sName, "default", 7u) == 0 ) {
					__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_SYNTAX,
						tName.iLine, tName.iColumn, tName.iOffset,
						"machine name 'default' is reserved");
					__xrtNetrcTokenUnit(&tKeyword);
					__xrtNetrcTokenUnit(&tName);
					goto fail;
				}
			}
			if ( xrtValueObjectHas(pHosts, (xstrview){ sName, iNameSize }) &&
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
			if ( pAuth == NULL || !xrtValueObjectSetTake(pHosts,
				(xstrview){ sName, iNameSize }, &pAuth) ) {
				xrtValueRelease(pAuth);
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			__xrtNetrcTokenUnit(&tName);
		} else if ( !tKeyword.bQuoted &&
			__xrtNetrcTokenEquals(&tKeyword, "macdef") ) {
			xvalue* pLines;
			if ( !__xrtNetrcNext(&tParser, &tName, &bEof) || bEof ||
				tName.iSize == 0u || __xrtNetrcIsTopLevel(&tName) ) {
				__xrtNetrcSetError(pContext->Error, XRT_NETRC_ERROR_SYNTAX,
					tKeyword.iLine, tKeyword.iColumn, tKeyword.iOffset,
					"macdef requires a non-empty name");
				__xrtNetrcTokenUnit(&tKeyword);
				__xrtNetrcTokenUnit(&tName);
				goto fail;
			}
			if ( xrtValueObjectHas(pMacros,
				(xstrview){ (const char*)tName.sText, tName.iSize }) &&
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
			if ( pLines == NULL || !xrtValueObjectSetTake(pMacros,
				(xstrview){ (const char*)tName.sText, tName.iSize }, &pLines) ) {
				xrtValueRelease(pLines);
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
	xrtValueRelease(pRoot);
	return NULL;
}

xvalue* xrtNetrcParse(xreader* pReader, bool bTakeOwnership,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	xrtnetrcerror tLocalError;
	xrt_netrc_context tContext;
	xbuffer* pInput = NULL;
	xvalue* pResult = NULL;
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	uint64 iLimit;
	size_t iReadLimit;
	const char* pData;
	size_t iDataSize;
	size_t iBadUtf8 = 0u;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtNetrcErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Error = pError;
	if ( !__xrtNetrcResolveOptions(pOptions, &tContext.Options, pError) ) {
		goto cleanup;
	}
	if ( pReader == NULL ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid netrc reader");
		goto cleanup;
	}
	iLimit = tContext.Options.tLimits.iMaxInputBytes;
	iReadLimit = iLimit == 0u || iLimit > SIZE_MAX - 1u ?
		SIZE_MAX - 1u : (size_t)iLimit;
	xrtClearError();
	if ( iLimit != 0u && xrtReaderSize(pReader, &iSize) &&
		xrtReaderTell(pReader, &iPosition) && iPosition <= iSize &&
		iSize - iPosition > iLimit ) {
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_LIMIT, 0u, 0u, 0u,
			"netrc input exceeds the configured limit");
		goto cleanup;
	}
	xrtClearError();
	pInput = xrtReaderReadAll(pReader, iReadLimit);
	if ( pInput == NULL ) {
		const xerror* pReadError = xrtGetError();
		xerrkind eKind = pReadError != NULL ? xrtErrorKind(pReadError) : XERR_IO;
		int iCode = eKind == XERR_RANGE ? XRT_NETRC_ERROR_LIMIT :
			(eKind == XERR_MEMORY ? XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO);
		__xrtNetrcSetError(pError, iCode, 0u, 0u, 0u,
			iCode == XRT_NETRC_ERROR_LIMIT ? "netrc input exceeds the configured limit" :
			(iCode == XRT_NETRC_ERROR_MEMORY ? "cannot allocate netrc input buffer" :
			"cannot read netrc stream"));
		goto cleanup;
	}
	pData = (const char*)pInput->Data;
	iDataSize = pInput->Size;
	if ( iDataSize >= 3u && (unsigned char)pData[0] == 0xefu &&
		(unsigned char)pData[1] == 0xbbu && (unsigned char)pData[2] == 0xbfu ) {
		pData += 3;
		iDataSize -= 3u;
	}
	if ( !xrtUtf8Valid((xstrview){ pData, iDataSize }, &iBadUtf8) ) {
		xrt_netrc_scanner tLocation;
		memset(&tLocation, 0, sizeof(tLocation));
		tLocation.pData = pData;
		tLocation.iSize = iDataSize;
		tLocation.iLine = 1u;
		tLocation.iColumn = 1u;
		while ( tLocation.iPos < iBadUtf8 ) { __xrtNetrcAdvance(&tLocation); }
		__xrtNetrcSetError(pError, XRT_NETRC_ERROR_UTF8,
			tLocation.iLine, tLocation.iColumn, tLocation.iPos,
			"netrc input is not valid UTF-8");
		goto cleanup;
	}
	pResult = __xrtNetrcParseDocument(pData, iDataSize, &tContext);

cleanup:
	xrtBufferDestroy(pInput);
	if ( bTakeOwnership ) {
		/* 关闭可以写入另一条 XRT 错误；不能只保住包级错误而丢失首因。 */
		xerror* pCause = pResult == NULL ? xrtTakeError() : NULL;
		bool bClosed = xrtReaderDestroy(pReader);
		if ( pCause != NULL ) { xrtSetErrorTake(pCause); }
		if ( !bClosed && pResult != NULL ) {
			const xerror* pCloseError = xrtGetError();
			int iCode = pCloseError != NULL && xrtErrorKind(pCloseError) == XERR_MEMORY ?
				XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO;
			/* 读取成功不代表拥有型流已经成功关闭。 */
			xrtValueRelease(pResult);
			pResult = NULL;
			__xrtNetrcSetError(pError, iCode, 0u, 0u, 0u,
				iCode == XRT_NETRC_ERROR_MEMORY ?
				"cannot allocate netrc reader close resources" :
				"cannot close netrc stream");
		}
	}
	return pResult;
}

xvalue* xrtNetrcParseMemory(const void* pData, uint64 iSize,
	const xrtnetrcoptions* pOptions, xrtnetrcerror* pError)
{
	if ( iSize > SIZE_MAX ) {
		if ( pError != NULL ) {
			xrtNetrcErrorInit(pError);
			__xrtNetrcSetError(pError, XRT_NETRC_ERROR_ARGUMENT,
				0u, 0u, 0u, "netrc input size is not representable");
		}
		return NULL;
	}
	if ( pData == NULL && iSize != 0u ) {
		if ( pError != NULL ) {
			xrtNetrcErrorInit(pError);
			__xrtNetrcSetError(pError, XRT_NETRC_ERROR_ARGUMENT,
				0u, 0u, 0u, "invalid netrc input");
		}
		return NULL;
	}
	xreader* pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize }) : NULL;
	if ( pReader == NULL ) {
		if ( pError != NULL ) {
			xrtNetrcErrorInit(pError);
			__xrtNetrcSetError(pError, XRT_NETRC_ERROR_MEMORY,
				0u, 0u, 0u, "cannot allocate netrc memory reader");
		}
		return NULL;
	}
	return xrtNetrcParse(pReader, true, pOptions, pError);
}

xvalue* xrtNetrcLoad(str sPath, const xrtnetrcoptions* pOptions,
	xrtnetrcerror* pError)
{
	xreader* pReader;
	if ( sPath == NULL ) { return xrtNetrcParse(NULL, false, pOptions, pError); }
	xrtClearError();
	pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		const xerror* pOpenError = xrtGetError();
		int iCode = pOpenError != NULL && xrtErrorKind(pOpenError) == XERR_MEMORY ?
			XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO;
		if ( pError != NULL ) {
			xrtNetrcErrorInit(pError);
			__xrtNetrcSetError(pError, iCode, 0u, 0u, 0u,
				iCode == XRT_NETRC_ERROR_MEMORY ?
				"cannot allocate netrc file reader" : "cannot open netrc file");
		}
		return NULL;
	}
	return xrtNetrcParse(pReader, true, pOptions, pError);
}
