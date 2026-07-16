#include "xrt_toml_internal.h"

#include <math.h>



bool __xrtTomlAppend(xbuffer pOutput, const void* pData, size_t iSize,
	xrt_toml_context* pContext)
{
	uint64 iLimit;
	if ( iSize == 0u ) { return TRUE; }
	if ( pOutput == NULL || pData == NULL || iSize > UINT32_MAX ) { return FALSE; }
	iLimit = pContext->Options.tLimits.iMaxOutputBytes;
	if ( iLimit != 0u && ((uint64)iSize > iLimit ||
		(uint64)pOutput->Length > iLimit - (uint64)iSize) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML output exceeds the configured limit");
		return FALSE;
	}
	if ( !xrtBufferAppend(pOutput, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot grow TOML output buffer");
		return FALSE;
	}
	return TRUE;
}

bool __xrtTomlAppendText(xbuffer pOutput, const char* sText,
	xrt_toml_context* pContext)
{
	return sText != NULL && __xrtTomlAppend(pOutput, sText, strlen(sText), pContext);
}

static bool __xrtTomlAppendKey(xbuffer pOutput, const char* sKey, size_t iSize,
	xrt_toml_context* pContext)
{
	size_t i;
	bool bBare = iSize != 0u;
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sKey[i];
		if ( !(c == '-' || c == '_' || (c >= '0' && c <= '9') ||
			(c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) ) {
			bBare = FALSE;
			break;
		}
	}
	if ( bBare ) { return __xrtTomlAppend(pOutput, sKey, iSize, pContext); }
	if ( !__xrtTomlAppendText(pOutput, "\"", pContext) ) { return FALSE; }
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sKey[i];
		char sEscape[8];
		if ( c == '\\' || c == '"' ) {
			sEscape[0] = '\\'; sEscape[1] = (char)c;
			if ( !__xrtTomlAppend(pOutput, sEscape, 2u, pContext) ) { return FALSE; }
		} else if ( c < 0x20u || c == 0x7fu ) {
			snprintf(sEscape, sizeof(sEscape), "\\u%04X", c);
			if ( !__xrtTomlAppend(pOutput, sEscape, 6u, pContext) ) { return FALSE; }
		} else if ( !__xrtTomlAppend(pOutput, &c, 1u, pContext) ) { return FALSE; }
	}
	return __xrtTomlAppendText(pOutput, "\"", pContext);
}

static bool __xrtTomlAppendString(xbuffer pOutput, const char* sText,
	size_t iSize, xrt_toml_context* pContext)
{
	size_t i;
	if ( !__xrtTomlAppendText(pOutput, "\"", pContext) ) { return FALSE; }
	for ( i = 0u; i < iSize; ++i ) {
		unsigned char c = (unsigned char)sText[i];
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
			if ( !__xrtTomlAppendText(pOutput, sEscape, pContext) ) { return FALSE; }
		} else if ( c < 0x20u || c == 0x7fu ) {
			snprintf(sUnicode, sizeof(sUnicode), "\\u%04X", c);
			if ( !__xrtTomlAppend(pOutput, sUnicode, 6u, pContext) ) { return FALSE; }
		} else if ( !__xrtTomlAppend(pOutput, &c, 1u, pContext) ) { return FALSE; }
	}
	return __xrtTomlAppendText(pOutput, "\"", pContext);
}

static bool __xrtTomlArrayIsTables(xvalue pValue)
{
	uint32 i;
	uint32 iCount = xvoArrayItemCount(pValue);
	if ( iCount == 0u ) { return FALSE; }
	for ( i = 0u; i < iCount; ++i ) {
		if ( xvoType(xvoArrayGetValue(pValue, i)) != XVO_DT_TABLE ) { return FALSE; }
	}
	return TRUE;
}

static bool __xrtTomlEncodeTime(xbuffer pOutput, xtime tValue,
	xrt_toml_context* pContext)
{
	char sText[48];
	int64 iYear;
	int iMonth, iDay, iHour, iMinute, iSecond;
	xrtDecodeSerial(tValue, &iYear, &iMonth, &iDay, &iHour, &iMinute,
		&iSecond, NULL, NULL);
	snprintf(sText, sizeof(sText), "%04lld-%02d-%02dT%02d:%02d:%02dZ",
		(long long)iYear, iMonth, iDay, iHour, iMinute, iSecond);
	return __xrtTomlAppendText(pOutput, sText, pContext);
}

typedef struct {
	xbuffer Output;
	xrt_toml_context* Context;
	uint32 Depth;
	bool First;
	bool Failed;
} xrt_toml_inline_table_context;

static bool __xrtTomlInlineTableItem(Dict_Key* pKey, ptr pRawValue, ptr pArg)
{
	xrt_toml_inline_table_context* pWalk = (xrt_toml_inline_table_context*)pArg;
	xvalue pValue = *(xvalue*)pRawValue;
	if ( pWalk->Failed ) { return TRUE; }
	if ( !pWalk->First &&
		!__xrtTomlAppendText(pWalk->Output, ", ", pWalk->Context) ) {
		pWalk->Failed = TRUE; return TRUE;
	}
	pWalk->First = FALSE;
	if ( !__xrtTomlAppendKey(pWalk->Output, (const char*)pKey->Key,
		pKey->KeyLen, pWalk->Context) ||
		!__xrtTomlAppendText(pWalk->Output, " = ", pWalk->Context) ||
		!__xrtTomlEncodeValue(pWalk->Output, pValue, pWalk->Context,
			pWalk->Depth + 1u, TRUE) ) {
		pWalk->Failed = TRUE; return TRUE;
	}
	return FALSE;
}

bool __xrtTomlEncodeValue(xbuffer pOutput, xvalue pValue,
	xrt_toml_context* pContext, uint32 iDepth, bool bInline)
{
	char sNumber[128];
	uint32 i;
	int iType;
	int iLocalKind;
	if ( !__xrtTomlLimitNode(pContext, iDepth, 0u, 0u) ) { return FALSE; }
	iType = xvoType(pValue);
	switch ( iType ) {
	case XVO_DT_BOOL:
		return __xrtTomlAppendText(pOutput, xvoGetBool(pValue) ? "true" : "false", pContext);
	case XVO_DT_INT:
		snprintf(sNumber, sizeof(sNumber), "%lld", (long long)xvoGetInt(pValue));
		return __xrtTomlAppendText(pOutput, sNumber, pContext);
	case XVO_DT_FLOAT: {
		double fValue = xvoGetFloat(pValue);
		if ( isnan(fValue) ) { return __xrtTomlAppendText(pOutput, "nan", pContext); }
		if ( isinf(fValue) ) {
			return __xrtTomlAppendText(pOutput, signbit(fValue) ? "-inf" : "inf", pContext);
		}
		if ( xrtNumToStr(fValue, sNumber) <= 0 ) { return FALSE; }
		if ( strchr(sNumber, '.') == NULL && strchr(sNumber, 'e') == NULL &&
			strchr(sNumber, 'E') == NULL ) {
			size_t iLength = strlen(sNumber);
			if ( iLength + 2u >= sizeof(sNumber) ) { return FALSE; }
			sNumber[iLength] = '.';
			sNumber[iLength + 1u] = '0';
			sNumber[iLength + 2u] = 0;
		}
		return __xrtTomlAppendText(pOutput, sNumber, pContext);
	}
	case XVO_DT_TEXT:
		return __xrtTomlAppendString(pOutput, (const char*)xvoGetText(pValue),
			xvoGetSize(pValue), pContext);
	case XVO_DT_TIME:
		return __xrtTomlEncodeTime(pOutput, xvoGetTime(pValue), pContext);
	case XVO_DT_ARRAY:
		if ( !__xrtTomlAppendText(pOutput, "[", pContext) ) { return FALSE; }
		for ( i = 0u; i < xvoArrayItemCount(pValue); ++i ) {
			if ( i != 0u && !__xrtTomlAppendText(pOutput, ", ", pContext) ) { return FALSE; }
			if ( !__xrtTomlEncodeValue(pOutput, xvoArrayGetValue(pValue, i),
				pContext, iDepth + 1u, TRUE) ) { return FALSE; }
		}
		return __xrtTomlAppendText(pOutput, "]", pContext);
	case XVO_DT_TABLE:
		if ( bInline ) {
			xrt_toml_inline_table_context tWalk = {
				pOutput, pContext, iDepth, TRUE, FALSE
			};
			if ( !__xrtTomlAppendText(pOutput, "{", pContext) ) { return FALSE; }
			xvoTableWalk(pValue, __xrtTomlInlineTableItem, &tWalk);
			return !tWalk.Failed && __xrtTomlAppendText(pOutput, "}", pContext);
		}
		break;
	case XVO_DT_CLASS:
		if ( xrtTomlIsDateTimeValue(pValue, &iLocalKind) ) {
			str sText = xrtTomlDateTimeToString(xrtTomlGetDateTime(pValue));
			bool bResult = sText != NULL && __xrtTomlAppendText(pOutput,
				(const char*)sText, pContext);
			xrtFree(sText);
			return bResult;
		}
		break;
	default: break;
	}
	__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_TYPE, 0u, 0u, 0u,
		"xvalue type %d cannot be encoded as TOML", iType);
	return FALSE;
}

static xbuffer __xrtTomlChildPath(xbuffer pPath, Dict_Key* pKey,
	xrt_toml_context* pContext)
{
	xbuffer pResult = xrtBufferCreate(64u);
	if ( pResult == NULL ) { return NULL; }
	if ( pPath != NULL && pPath->Length != 0u &&
		(!__xrtTomlAppend(pResult, pPath->Buffer, pPath->Length, pContext) ||
		 !__xrtTomlAppendText(pResult, ".", pContext)) ) {
		xrtBufferDestroy(pResult); return NULL;
	}
	if ( !__xrtTomlAppendKey(pResult, (const char*)pKey->Key,
		pKey->KeyLen, pContext) ) {
		xrtBufferDestroy(pResult); return NULL;
	}
	return pResult;
}

typedef struct {
	xbuffer Output;
	xbuffer Path;
	xrt_toml_context* Context;
	uint32 Depth;
	int Pass;
	bool Failed;
} xrt_toml_table_context;

static bool __xrtTomlEncodeTableBody(xbuffer pOutput, xvalue pTable,
	xbuffer pPath, xrt_toml_context* pContext, uint32 iDepth);

static bool __xrtTomlTableItem(Dict_Key* pKey, ptr pRawValue, ptr pArg)
{
	xrt_toml_table_context* pWalk = (xrt_toml_table_context*)pArg;
	xvalue pValue = *(xvalue*)pRawValue;
	int iType = xvoType(pValue);
	bool bArrayTable = iType == XVO_DT_ARRAY && __xrtTomlArrayIsTables(pValue);
	xbuffer pChildPath;
	uint32 i;
	if ( pWalk->Failed ) { return TRUE; }
	if ( pWalk->Pass == 0 && iType != XVO_DT_TABLE && !bArrayTable ) {
		if ( !__xrtTomlAppendKey(pWalk->Output, (const char*)pKey->Key,
			pKey->KeyLen, pWalk->Context) ||
			!__xrtTomlAppendText(pWalk->Output, " = ", pWalk->Context) ||
			!__xrtTomlEncodeValue(pWalk->Output, pValue, pWalk->Context,
				pWalk->Depth + 1u, FALSE) ||
			!__xrtTomlAppendText(pWalk->Output, "\n", pWalk->Context) ) {
			pWalk->Failed = TRUE;
		}
		return pWalk->Failed;
	}
	if ( (pWalk->Pass == 1 && iType != XVO_DT_TABLE) ||
		(pWalk->Pass == 2 && !bArrayTable) ) { return FALSE; }
	if ( pWalk->Pass == 0 ) { return FALSE; }
	pChildPath = __xrtTomlChildPath(pWalk->Path, pKey, pWalk->Context);
	if ( pChildPath == NULL ) { pWalk->Failed = TRUE; return TRUE; }
	if ( pWalk->Pass == 1 ) {
		if ( !__xrtTomlAppendText(pWalk->Output, "\n[", pWalk->Context) ||
			!__xrtTomlAppend(pWalk->Output, pChildPath->Buffer,
				pChildPath->Length, pWalk->Context) ||
			!__xrtTomlAppendText(pWalk->Output, "]\n", pWalk->Context) ||
			!__xrtTomlEncodeTableBody(pWalk->Output, pValue, pChildPath,
				pWalk->Context, pWalk->Depth + 1u) ) {
			pWalk->Failed = TRUE;
		}
	} else {
		for ( i = 0u; i < xvoArrayItemCount(pValue) && !pWalk->Failed; ++i ) {
			if ( !__xrtTomlAppendText(pWalk->Output, "\n[[", pWalk->Context) ||
				!__xrtTomlAppend(pWalk->Output, pChildPath->Buffer,
					pChildPath->Length, pWalk->Context) ||
				!__xrtTomlAppendText(pWalk->Output, "]]\n", pWalk->Context) ||
				!__xrtTomlEncodeTableBody(pWalk->Output,
					xvoArrayGetValue(pValue, i), pChildPath, pWalk->Context,
					pWalk->Depth + 1u) ) {
				pWalk->Failed = TRUE;
			}
		}
	}
	xrtBufferDestroy(pChildPath);
	return pWalk->Failed;
}

static bool __xrtTomlEncodeTableBody(xbuffer pOutput, xvalue pTable,
	xbuffer pPath, xrt_toml_context* pContext, uint32 iDepth)
{
	xrt_toml_table_context tWalk = { pOutput, pPath, pContext, iDepth, 0, FALSE };
	int iPass;
	if ( !__xrtTomlLimitNode(pContext, iDepth, 0u, 0u) ) { return FALSE; }
	for ( iPass = 0; iPass < 3 && !tWalk.Failed; ++iPass ) {
		tWalk.Pass = iPass;
		xvoTableWalk(pTable, __xrtTomlTableItem, &tWalk);
	}
	return !tWalk.Failed;
}

XXAPI xbuffer xrtTomlStringify(xvalue pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrt_toml_context tContext;
	xbuffer pOutput;
	xbuffer pPath;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtTomlErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	xrtTomlOptionsInit(&tContext.Options);
	if ( pOptions != NULL && pOptions->iSize >= sizeof(xrttomloptions) &&
		pOptions->iVersion == XRT_TOML_OPTIONS_VERSION ) { tContext.Options = *pOptions; }
	tContext.Error = pError;
	if ( pValue == NULL || xvoType(pValue) != XVO_DT_TABLE ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_TYPE, 0u, 0u, 0u,
			"TOML document root must be a dict");
		return NULL;
	}
	pOutput = xrtBufferCreate(1024u);
	pPath = xrtBufferCreate(64u);
	if ( pOutput == NULL || pPath == NULL ||
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

XXAPI bool xrtTomlWrite(xvalue pValue, xwriter pWriter, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrttomlerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer pOutput = xrtTomlStringify(pValue, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && pWriter != NULL &&
		xrtWriterWriteAll(pWriter, pOutput->Buffer, pOutput->Length) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_TOML_OK ) {
		__xrtTomlSetError(pResolvedError, XRT_TOML_ERROR_IO, 0u, 0u, 0u,
			"cannot write TOML stream");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership ) { xrtWriterDestroy(pWriter); }
	return bResult;
}

XXAPI bool xrtTomlSave(str sPath, xvalue pValue, const xrttomloptions* pOptions,
	xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrttomlerror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer pOutput = xrtTomlStringify(pValue, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && sPath != NULL &&
		xrtFileWriteAllAtomic(sPath, (str)pOutput->Buffer, pOutput->Length,
			XRT_CP_BINARY);
	if ( !bResult && pResolvedError->iCode == XRT_TOML_OK ) {
		__xrtTomlSetError(pResolvedError, XRT_TOML_ERROR_IO, 0u, 0u, 0u,
			"cannot save TOML file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
