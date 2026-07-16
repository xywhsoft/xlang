#include "xrt_toml_internal.h"

#include <limits.h>
#include <stdlib.h>



XXAPI void xrtTomlOptionsInit(xrttomloptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_TOML_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI void xrtTomlErrorInit(xrttomlerror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

void __xrtTomlSetError(xrttomlerror* pError, int iCode, uint64 iLine,
	uint64 iColumn, uint64 iOffset, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pError == NULL || pError->iCode != XRT_TOML_OK ) { return; }
	pError->iCode = iCode;
	pError->iLine = iLine;
	pError->iColumn = iColumn;
	pError->iOffset = iOffset;
	va_start(tArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	va_end(tArgs);
}

static xrttomloptions __xrtTomlOptions(const xrttomloptions* pOptions)
{
	xrttomloptions tResult;
	xrtTomlOptionsInit(&tResult);
	if ( pOptions != NULL && pOptions->iSize >= sizeof(xrttomloptions) &&
		pOptions->iVersion == XRT_TOML_OPTIONS_VERSION ) {
		tResult = *pOptions;
	}
	return tResult;
}

bool __xrtTomlLimitNode(xrt_toml_context* pContext, uint32 iDepth,
	uint64 iLine, uint64 iColumn)
{
	if ( pContext->Options.tLimits.iMaxDepth != 0u &&
		iDepth > pContext->Options.tLimits.iMaxDepth ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, iLine,
			iColumn, 0u, "TOML nesting depth exceeds the configured limit");
		return FALSE;
	}
	pContext->Nodes += 1u;
	if ( pContext->Options.tLimits.iMaxNodes != 0u &&
		pContext->Nodes > pContext->Options.tLimits.iMaxNodes ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, iLine,
			iColumn, 0u, "TOML node count exceeds the configured limit");
		return FALSE;
	}
	return TRUE;
}

static xvalue __xrtTomlConvertDateTime(const toml_datum_t* pDatum)
{
	switch ( pDatum->type ) {
	case TOML_DATE:
		return xrtTomlCreateDate(pDatum->u.ts.year, pDatum->u.ts.month,
			pDatum->u.ts.day);
	case TOML_TIME:
		return xrtTomlCreateTime(pDatum->u.ts.hour, pDatum->u.ts.minute,
			pDatum->u.ts.second, pDatum->u.ts.usec);
	case TOML_DATETIME:
		return xrtTomlCreateDateTime(pDatum->u.ts.year, pDatum->u.ts.month,
			pDatum->u.ts.day, pDatum->u.ts.hour, pDatum->u.ts.minute,
			pDatum->u.ts.second, pDatum->u.ts.usec);
	case TOML_DATETIMETZ:
		/* XRT time 为秒精度；有小数秒时使用专用值，禁止静默丢失精度。 */
		if ( pDatum->u.ts.usec != 0 ) {
			return xrtTomlCreateOffsetDateTime(pDatum->u.ts.year,
				pDatum->u.ts.month, pDatum->u.ts.day, pDatum->u.ts.hour,
				pDatum->u.ts.minute, pDatum->u.ts.second,
				pDatum->u.ts.usec, pDatum->u.ts.tz);
		}
		return xvoCreateTime(xrtDateTimeSerial(pDatum->u.ts.year,
			pDatum->u.ts.month, pDatum->u.ts.day, pDatum->u.ts.hour,
			pDatum->u.ts.minute, pDatum->u.ts.second) -
			((xtime)pDatum->u.ts.tz * XRT_TIME_MINUTE));
	default:
		return NULL;
	}
}

xvalue __xrtTomlConvertDatum(xrt_toml_context* pContext,
	const toml_datum_t* pDatum, uint32 iDepth)
{
	xvalue pResult = NULL;
	int i;
	if ( pDatum == NULL || !__xrtTomlLimitNode(pContext, iDepth,
		(uint64)pDatum->lineno, (uint64)pDatum->colno) ) { return NULL; }
	switch ( pDatum->type ) {
	case TOML_STRING:
		if ( pDatum->u.str.len < 0 ||
			(pContext->Options.tLimits.iMaxItemBytes != 0u &&
			 (uint64)pDatum->u.str.len > pContext->Options.tLimits.iMaxItemBytes) ) {
			__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT,
				pDatum->lineno, pDatum->colno, 0u,
				"TOML string exceeds the configured item limit");
			return NULL;
		}
		return xvoCreateText((ptr)pDatum->u.str.ptr,
			(uint32)pDatum->u.str.len, FALSE);
	case TOML_INT64: return xvoCreateInt(pDatum->u.int64);
	case TOML_FP64: return xvoCreateFloat(pDatum->u.fp64);
	case TOML_BOOLEAN: return xvoCreateBool(pDatum->u.boolean);
	case TOML_DATE:
	case TOML_TIME:
	case TOML_DATETIME:
	case TOML_DATETIMETZ:
		pResult = __xrtTomlConvertDateTime(pDatum);
		break;
	case TOML_ARRAY:
		if ( pDatum->u.arr.size < 0 ||
			(pContext->Options.tLimits.iMaxEntries != 0u &&
			 (uint64)pDatum->u.arr.size > pContext->Options.tLimits.iMaxEntries) ) {
			__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT,
				pDatum->lineno, pDatum->colno, 0u,
				"TOML array exceeds the configured entry limit");
			return NULL;
		}
		pResult = xvoCreateArray();
		for ( i = 0; pResult != NULL && i < pDatum->u.arr.size; ++i ) {
			xvalue pItem = __xrtTomlConvertDatum(pContext,
				&pDatum->u.arr.elem[i], iDepth + 1u);
			if ( pItem == NULL || !xvoArrayAppendValue(pResult, pItem, TRUE) ) {
				if ( pItem != NULL ) { xvoUnref(pItem); }
				xvoUnref(pResult);
				pResult = NULL;
			}
		}
		break;
	case TOML_TABLE:
		if ( pDatum->u.tab.size < 0 ||
			(pContext->Options.tLimits.iMaxEntries != 0u &&
			 (uint64)pDatum->u.tab.size > pContext->Options.tLimits.iMaxEntries) ) {
			__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT,
				pDatum->lineno, pDatum->colno, 0u,
				"TOML table exceeds the configured entry limit");
			return NULL;
		}
		pResult = xvoCreateTable();
		for ( i = 0; pResult != NULL && i < pDatum->u.tab.size; ++i ) {
			xvalue pItem;
			int iKeySize = pDatum->u.tab.len[i];
			if ( iKeySize < 0 || (pContext->Options.tLimits.iMaxItemBytes != 0u &&
				(uint64)iKeySize > pContext->Options.tLimits.iMaxItemBytes) ) {
				__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT,
					pDatum->u.tab.value[i].lineno, pDatum->u.tab.value[i].colno,
					0u, "TOML key exceeds the configured item limit");
				xvoUnref(pResult);
				return NULL;
			}
			pItem = __xrtTomlConvertDatum(pContext, &pDatum->u.tab.value[i],
				iDepth + 1u);
			if ( pItem == NULL || !xvoTableSetValue(pResult,
				pDatum->u.tab.key[i], (uint32)iKeySize, pItem, TRUE) ) {
				if ( pItem != NULL ) { xvoUnref(pItem); }
				xvoUnref(pResult);
				pResult = NULL;
			}
		}
		break;
	default:
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_TYPE,
			pDatum->lineno, pDatum->colno, 0u, "unsupported TOML value type");
		return NULL;
	}
	if ( pResult == NULL && pContext->Error->iCode == XRT_TOML_OK ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_MEMORY,
			pDatum->lineno, pDatum->colno, 0u, "cannot allocate TOML value");
	}
	return pResult;
}

static uint64 __xrtTomlErrorLine(const char* sMessage)
{
	const char* pLine = sMessage != NULL ? strstr(sMessage, "(line ") : NULL;
	return pLine != NULL ? (uint64)strtoull(pLine + 6, NULL, 10) : 0u;
}

XXAPI xvalue xrtTomlParse(xreader pReader, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrt_toml_context tContext;
	xbuffer pInput = NULL;
	xvalue pResult = NULL;
	toml_result_t tParsed;
	uint64 iSize = 0u;
	uint64 iLimit;
	uint32 iParseSize;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtTomlErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Options = __xrtTomlOptions(pOptions);
	tContext.Error = pError;
	if ( pReader == NULL ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid TOML reader");
		goto cleanup;
	}
	iLimit = tContext.Options.tLimits.iMaxInputBytes;
	if ( xrtReaderSize(pReader, &iSize) &&
		((iLimit != 0u && iSize > iLimit) || iSize > INT_MAX) ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML input exceeds the configured or parser limit");
		goto cleanup;
	}
	pInput = xrtReaderReadAll(pReader, iLimit);
	if ( pInput == NULL ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_IO, 0u, 0u, 0u,
			"cannot read TOML stream or input limit was exceeded");
		goto cleanup;
	}
	if ( pInput->Length > INT_MAX ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML input exceeds the parser limit");
		goto cleanup;
	}
	if ( !xrtIsUTF8((str)pInput->Buffer, pInput->Length) ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_FORMAT, 1u, 1u, 0u,
			"TOML input is not valid UTF-8");
		goto cleanup;
	}
	iParseSize = pInput->Length;
	/* tomlc17 明确要求 len 后还有一个 NUL；流和 Buffer 不承诺该哨兵。 */
	if ( !xrtBufferAppend(pInput, "\0", 1u, XBUF_BINARY) ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot terminate TOML parser input");
		goto cleanup;
	}
	tParsed = toml_parse((const char*)pInput->Buffer, (int)iParseSize);
	if ( !tParsed.ok ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_FORMAT,
			__xrtTomlErrorLine(tParsed.errmsg), 0u, 0u, "%s",
			tParsed.errmsg[0] != 0 ? tParsed.errmsg : "invalid TOML input");
		toml_free(tParsed);
		goto cleanup;
	}
	pResult = __xrtTomlConvertDatum(&tContext, &tParsed.toptab, 1u);
	toml_free(tParsed);

cleanup:
	xrtBufferDestroy(pInput);
	if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
	return pResult;
}

XXAPI xvalue xrtTomlParseMemory(const void* pData, uint64 iSize,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xreader pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory(pData, iSize) : NULL;
	return xrtTomlParse(pReader, TRUE, pOptions, pError);
}

XXAPI xvalue xrtTomlLoad(str sPath, const xrttomloptions* pOptions,
	xrttomlerror* pError)
{
	xreader pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtTomlParse(pReader, TRUE, pOptions, pError);
}
