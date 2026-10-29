#include "xrt_toml_internal.h"

bool __xrtTomlAppend(xbuffer* pOutput, const void* pData, size_t iSize,
	xrt_toml_context* pContext)
{
	uint64 iLimit;
	if ( iSize == 0u ) { return true; }
	if ( pOutput == NULL || pData == NULL || pContext == NULL ) { return false; }
	iLimit = pContext->Options.tLimits.iMaxOutputBytes;
	if ( iLimit != 0u && ((uint64)pOutput->Size > iLimit ||
		(uint64)iSize > iLimit - (uint64)pOutput->Size) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML output exceeds the configured limit");
		return false;
	}
	if ( !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot grow TOML output");
		return false;
	}
	return true;
}

bool __xrtTomlAppendText(xbuffer* pOutput, const char* sText,
	xrt_toml_context* pContext)
{
	return sText != NULL && __xrtTomlAppend(pOutput, sText, strlen(sText),
		pContext);
}

static bool __xrtTomlLimitEntries(const xvalue* pValue,
	xrt_toml_context* pContext, const char* sKind)
{
	uint64 iLimit = pContext->Options.tLimits.iMaxEntries;
	if ( iLimit == 0u || (uint64)xrtValueCount(pValue) <= iLimit ) { return true; }
	__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
		"TOML %s exceeds the configured entry limit", sKind);
	return false;
}

static bool __xrtTomlAppendKey(xbuffer* pOutput, xstrview Key,
	xrt_toml_context* pContext)
{
	size_t i;
	bool bBare = Key.Size != 0u;
	if ( pContext->Options.tLimits.iMaxItemBytes != 0u &&
		(uint64)Key.Size > pContext->Options.tLimits.iMaxItemBytes ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML key exceeds the configured item limit");
		return false;
	}
	if ( !xrtUtf8Valid(Key, NULL) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_FORMAT, 0u, 0u, 0u,
			"TOML key is not valid UTF-8");
		return false;
	}
	for ( i = 0u; i < Key.Size; ++i ) {
		unsigned char c = (unsigned char)Key.Data[i];
		if ( !(c == '-' || c == '_' || (c >= '0' && c <= '9') ||
			(c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) ) {
			bBare = false;
			break;
		}
	}
	if ( bBare ) { return __xrtTomlAppend(pOutput, Key.Data, Key.Size, pContext); }
	if ( !__xrtTomlAppendText(pOutput, "\"", pContext) ) { return false; }
	for ( i = 0u; i < Key.Size; ++i ) {
		unsigned char c = (unsigned char)Key.Data[i];
		char sEscape[8];
		if ( c == '\\' || c == '"' ) {
			sEscape[0] = '\\';
			sEscape[1] = (char)c;
			if ( !__xrtTomlAppend(pOutput, sEscape, 2u, pContext) ) { return false; }
		} else if ( c < 0x20u || c == 0x7fu ) {
			snprintf(sEscape, sizeof(sEscape), "\\u%04X", c);
			if ( !__xrtTomlAppend(pOutput, sEscape, 6u, pContext) ) { return false; }
		} else if ( !__xrtTomlAppend(pOutput, &c, 1u, pContext) ) {
			return false;
		}
	}
	return __xrtTomlAppendText(pOutput, "\"", pContext);
}

static bool __xrtTomlAppendString(xbuffer* pOutput, xstrview Text,
	xrt_toml_context* pContext)
{
	size_t i;
	if ( pContext->Options.tLimits.iMaxItemBytes != 0u &&
		(uint64)Text.Size > pContext->Options.tLimits.iMaxItemBytes ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML string exceeds the configured item limit");
		return false;
	}
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_FORMAT, 0u, 0u, 0u,
			"TOML string is not valid UTF-8");
		return false;
	}
	if ( !__xrtTomlAppendText(pOutput, "\"", pContext) ) { return false; }
	for ( i = 0u; i < Text.Size; ++i ) {
		unsigned char c = (unsigned char)Text.Data[i];
		const char* sEscape = NULL;
		char sUnicode[8];
		switch ( c ) {
		case '\b': sEscape = "\\b"; break;
		case '\t': sEscape = "\\t"; break;
		case '\n': sEscape = "\\n"; break;
		case '\f': sEscape = "\\f"; break;
		case '\r': sEscape = "\\r"; break;
		case '"': sEscape = "\\\""; break;
		case '\\': sEscape = "\\\\"; break;
		default: break;
		}
		if ( sEscape != NULL ) {
			if ( !__xrtTomlAppendText(pOutput, sEscape, pContext) ) { return false; }
		} else if ( c < 0x20u || c == 0x7fu ) {
			snprintf(sUnicode, sizeof(sUnicode), "\\u%04X", c);
			if ( !__xrtTomlAppend(pOutput, sUnicode, 6u, pContext) ) { return false; }
		} else if ( !__xrtTomlAppend(pOutput, &c, 1u, pContext) ) {
			return false;
		}
	}
	return __xrtTomlAppendText(pOutput, "\"", pContext);
}

static bool __xrtTomlIsDateTime(const xvalue* pValue)
{
	return xrtTomlIsDateTimeValue(pValue, NULL);
}

static bool __xrtTomlArrayIsTables(const xvalue* pValue)
{
	size_t i;
	size_t iCount = xrtValueCount(pValue);
	if ( iCount == 0u ) { return false; }
	for ( i = 0u; i < iCount; ++i ) {
		xvalue* pItem = xrtValueArrayGet(pValue, i);
		if ( xrtValueType(pItem) != XVALUE_OBJECT || __xrtTomlIsDateTime(pItem) ) {
			return false;
		}
	}
	return true;
}

static bool __xrtTomlEncodeTime(xbuffer* pOutput, xtime Time,
	xrt_toml_context* pContext)
{
	xdatetime DateTime;
	char sText[64];
	if ( !xrtTimeSplit(Time, &DateTime) ) { return false; }
	snprintf(sText, sizeof(sText), "%04lld-%02d-%02dT%02d:%02d:%02d.%06dZ",
		(long long)DateTime.Year, DateTime.Month, DateTime.Day, DateTime.Hour,
		DateTime.Minute, DateTime.Second, DateTime.Microsecond);
	return __xrtTomlAppendText(pOutput, sText, pContext);
}

bool __xrtTomlEncodeValue(xbuffer* pOutput, const xvalue* pValue,
	xrt_toml_context* pContext, uint32 iDepth, bool bInline);

static bool __xrtTomlEncodeObjectInline(xbuffer* pOutput, const xvalue* pObject,
	xrt_toml_context* pContext, uint32 iDepth)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pItem;
	bool bFirst = true;
	bool bResult = true;

	if ( !__xrtTomlLimitEntries(pObject, pContext, "table") ) { return false; }
	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pObject, &Iterator) ||
		!__xrtTomlAppendText(pOutput, "{", pContext) ) {
		return false;
	}
	while ( (pItem = xrtValueIterNext(&Iterator, &Key)) != NULL ) {
		if ( Key.Type != XVALUE_KEY_STRING ||
			(!bFirst && !__xrtTomlAppendText(pOutput, ", ", pContext)) ||
			!__xrtTomlAppendKey(pOutput, Key.String, pContext) ||
			!__xrtTomlAppendText(pOutput, " = ", pContext) ||
			!__xrtTomlEncodeValue(pOutput, pItem, pContext, iDepth + 1u, true) ) {
			bResult = false;
			break;
		}
		bFirst = false;
	}
	xrtValueIterEnd(&Iterator);
	return bResult && __xrtTomlAppendText(pOutput, "}", pContext);
}

bool __xrtTomlEncodeValue(xbuffer* pOutput, const xvalue* pValue,
	xrt_toml_context* pContext, uint32 iDepth, bool bInline)
{
	xvaluetype Type;
	char sNumber[128];
	if ( pValue == NULL || !__xrtTomlLimitNode(pContext, iDepth, 0u, 0u) ) {
		return false;
	}
	if ( __xrtTomlIsDateTime(pValue) ) {
		xrttomldatetime DateTime;
		str sText;
		if ( !xrtTomlGetDateTime(pValue, &DateTime) ||
			(sText = xrtTomlDateTimeToString(&DateTime)) == NULL ) {
			return false;
		}
		{
			bool bResult = __xrtTomlAppendText(pOutput, sText, pContext);
			xrtFree(sText);
			return bResult;
		}
	}
	Type = xrtValueType(pValue);
	switch ( Type ) {
	case XVALUE_BOOL:
	{
		bool bValue;
		return xrtValueGetBool(pValue, &bValue) && __xrtTomlAppendText(pOutput,
			bValue ? "true" : "false", pContext);
	}
	case XVALUE_INT:
	{
		int64 iValue;
		if ( !xrtValueGetInt(pValue, &iValue) ) { return false; }
		snprintf(sNumber, sizeof(sNumber), "%lld", (long long)iValue);
		return __xrtTomlAppendText(pOutput, sNumber, pContext);
	}
	case XVALUE_FLOAT:
	{
		double fValue;
		size_t iSize;
		if ( !xrtValueGetFloat(pValue, &fValue) ) { return false; }
		if ( !xrtNumWrite(fValue, sNumber, sizeof(sNumber), &iSize, 0u) ) {
			__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_TYPE, 0u, 0u, 0u,
				"cannot format TOML float");
			return false;
		}
		return __xrtTomlAppend(pOutput, sNumber, iSize, pContext);
	}
	case XVALUE_STRING:
	{
		xstrview Text;
		return xrtValueGetString(pValue, &Text) &&
			__xrtTomlAppendString(pOutput, Text, pContext);
	}
	case XVALUE_TIME:
	{
		xtime Time;
		return xrtValueGetTime(pValue, &Time) &&
			__xrtTomlEncodeTime(pOutput, Time, pContext);
	}
	case XVALUE_ARRAY:
	{
		size_t i;
		if ( !__xrtTomlLimitEntries(pValue, pContext, "array") ) {
			return false;
		}
		if ( !__xrtTomlAppendText(pOutput, "[", pContext) ) { return false; }
		for ( i = 0u; i < xrtValueCount(pValue); ++i ) {
			if ( (i != 0u && !__xrtTomlAppendText(pOutput, ", ", pContext)) ||
				!__xrtTomlEncodeValue(pOutput, xrtValueArrayGet(pValue, i),
					pContext, iDepth + 1u, true) ) {
				return false;
			}
		}
		return __xrtTomlAppendText(pOutput, "]", pContext);
	}
	case XVALUE_OBJECT:
		if ( bInline ) {
			return __xrtTomlEncodeObjectInline(pOutput, pValue, pContext, iDepth);
		}
		break;
	default:
		break;
	}
	__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_TYPE, 0u, 0u, 0u,
		"xvalue type %d cannot be encoded as TOML", (int)Type);
	return false;
}

static xbuffer* __xrtTomlChildPath(const xbuffer* pPath, xstrview Key,
	xrt_toml_context* pContext)
{
	xbuffer* pResult = xrtBufferCreate();
	if ( pResult == NULL || !xrtBufferReserve(pResult, 64u) ||
		(pPath != NULL && pPath->Size != 0u &&
		 (!__xrtTomlAppend(pResult, pPath->Data, pPath->Size, pContext) ||
		  !__xrtTomlAppendText(pResult, ".", pContext))) ||
		!__xrtTomlAppendKey(pResult, Key, pContext) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	return pResult;
}

static bool __xrtTomlEncodeTableBody(xbuffer* pOutput, const xvalue* pTable,
	const xbuffer* pPath, xrt_toml_context* pContext, uint32 iDepth);

static bool __xrtTomlEncodeTablePass(xbuffer* pOutput, const xvalue* pTable,
	const xbuffer* pPath, xrt_toml_context* pContext, uint32 iDepth, int iPass)
{
	xvalueiter Iterator;
	xvaluekey Key;
	xvalue* pValue;
	bool bResult = true;

	memset(&Iterator, 0, sizeof(Iterator));
	if ( !xrtValueIterBegin(pTable, &Iterator) ) { return false; }
	while ( (pValue = xrtValueIterNext(&Iterator, &Key)) != NULL ) {
		xvaluetype Type = xrtValueType(pValue);
		bool bTable = Type == XVALUE_OBJECT && !__xrtTomlIsDateTime(pValue);
		bool bArrayTable = Type == XVALUE_ARRAY && __xrtTomlArrayIsTables(pValue);
		xbuffer* pChildPath;
		size_t i;
		if ( Key.Type != XVALUE_KEY_STRING ) { bResult = false; break; }
		if ( iPass == 0 ) {
			if ( bTable || bArrayTable ) { continue; }
			if ( !__xrtTomlAppendKey(pOutput, Key.String, pContext) ||
				!__xrtTomlAppendText(pOutput, " = ", pContext) ||
				!__xrtTomlEncodeValue(pOutput, pValue, pContext, iDepth + 1u,
					false) || !__xrtTomlAppendText(pOutput, "\n", pContext) ) {
				bResult = false;
				break;
			}
			continue;
		}
		if ( (iPass == 1 && !bTable) || (iPass == 2 && !bArrayTable) ) {
			continue;
		}
		if ( iPass == 2 &&
			(!__xrtTomlLimitNode(pContext, iDepth + 1u, 0u, 0u) ||
			 !__xrtTomlLimitEntries(pValue, pContext, "array")) ) {
			bResult = false;
			break;
		}
		pChildPath = __xrtTomlChildPath(pPath, Key.String, pContext);
		if ( pChildPath == NULL ) { bResult = false; break; }
		if ( iPass == 1 ) {
			bResult = __xrtTomlAppendText(pOutput, "\n[", pContext) &&
				__xrtTomlAppend(pOutput, pChildPath->Data, pChildPath->Size,
					pContext) && __xrtTomlAppendText(pOutput, "]\n", pContext) &&
				__xrtTomlEncodeTableBody(pOutput, pValue, pChildPath, pContext,
					iDepth + 1u);
		} else {
			for ( i = 0u; bResult && i < xrtValueCount(pValue); ++i ) {
				bResult = __xrtTomlAppendText(pOutput, "\n[[", pContext) &&
					__xrtTomlAppend(pOutput, pChildPath->Data, pChildPath->Size,
						pContext) && __xrtTomlAppendText(pOutput, "]]\n", pContext) &&
					__xrtTomlEncodeTableBody(pOutput, xrtValueArrayGet(pValue, i),
						pChildPath, pContext, iDepth + 2u);
			}
		}
		xrtBufferDestroy(pChildPath);
		if ( !bResult ) { break; }
	}
	xrtValueIterEnd(&Iterator);
	return bResult;
}

static bool __xrtTomlEncodeTableBody(xbuffer* pOutput, const xvalue* pTable,
	const xbuffer* pPath, xrt_toml_context* pContext, uint32 iDepth)
{
	int iPass;
	if ( xrtValueType(pTable) != XVALUE_OBJECT || __xrtTomlIsDateTime(pTable) ||
		!__xrtTomlLimitNode(pContext, iDepth, 0u, 0u) ||
		!__xrtTomlLimitEntries(pTable, pContext, "table") ) {
		return false;
	}
	for ( iPass = 0; iPass < 3; ++iPass ) {
		if ( !__xrtTomlEncodeTablePass(pOutput, pTable, pPath, pContext,
			iDepth, iPass) ) {
			return false;
		}
	}
	return true;
}

XRT_API xbuffer* xrtTomlStringify(xvalue* pValue,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrt_toml_context tContext;
	xbuffer* pOutput;
	xbuffer* pPath;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtTomlErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Error = pError;
	if ( !__xrtTomlResolveOptions(pOptions, &tContext.Options, pError) ) {
		return NULL;
	}
	if ( xrtValueType(pValue) != XVALUE_OBJECT || __xrtTomlIsDateTime(pValue) ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_TYPE, 0u, 0u, 0u,
			"TOML document root must be a dict");
		return NULL;
	}
	pOutput = xrtBufferCreate();
	pPath = xrtBufferCreate();
	if ( pOutput == NULL || pPath == NULL || !xrtBufferReserve(pOutput, 1024u) ||
		!xrtBufferReserve(pPath, 64u) ||
		!__xrtTomlEncodeTableBody(pOutput, pValue, pPath, &tContext, 1u) ) {
		if ( pError->iCode == XRT_TOML_OK ) {
			__xrtTomlSetError(pError, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
				"cannot allocate TOML encoder state");
		}
		xrtBufferDestroy(pOutput);
		pOutput = NULL;
	}
	xrtBufferDestroy(pPath);
	return pOutput;
}

/* 写入器可能因分配失败而报错，不能把底层内存错误统一改成 IO。 */
static int __xrtTomlWriteFailureCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_TOML_ERROR_MEMORY : XRT_TOML_ERROR_IO;
}

XRT_API bool xrtTomlWrite(xvalue* pValue, xwriter* pWriter,
	bool bTakeOwnership, const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrttomlerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtTomlStringify(pValue, pOptions, pResolvedError);
	if ( pOutput != NULL ) { xrtClearError(); }
	bool bResult = pOutput != NULL && pWriter != NULL &&
		xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_TOML_OK ) {
		__xrtTomlSetError(pResolvedError, __xrtTomlWriteFailureCode(), 0u, 0u, 0u,
			"cannot write TOML stream");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership && !xrtWriterDestroy(pWriter) ) {
		if ( bResult ) {
			__xrtTomlSetError(pResolvedError,
				__xrtTomlWriteFailureCode(),
				0u, 0u, 0u,
				"cannot close TOML stream");
		}
		bResult = false;
	}
	return bResult;
}

XRT_API bool xrtTomlSave(cstr sPath, xvalue* pValue,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrttomlerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtTomlStringify(pValue, pOptions, pResolvedError);
	if ( pOutput != NULL ) { xrtClearError(); }
	bool bResult = pOutput != NULL && sPath != NULL &&
		xrtFileWriteAtomic(sPath, (xbytesview){ pOutput->Data, pOutput->Size });
	if ( !bResult && pResolvedError->iCode == XRT_TOML_OK ) {
		__xrtTomlSetError(pResolvedError, __xrtTomlWriteFailureCode(), 0u, 0u, 0u,
			"cannot save TOML file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
