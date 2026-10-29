#include "xrt_toml_internal.h"

#include <limits.h>
#include <stdlib.h>



XRT_API void xrtTomlOptionsInit(xrttomloptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_TOML_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxCompressionRatio = 0u;
}

XRT_API void xrtTomlErrorInit(xrttomlerror* pError)
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

bool __xrtTomlResolveOptions(const xrttomloptions* pOptions,
	xrttomloptions* pResolved, xrttomlerror* pError)
{
	xrtTomlOptionsInit(pResolved);
	if ( pOptions == NULL ) { return true; }
	if ( pOptions->iSize < sizeof(*pOptions) ||
		pOptions->iVersion != XRT_TOML_OPTIONS_VERSION ||
		pOptions->tLimits.iSize < sizeof(pOptions->tLimits) ||
		pOptions->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pOptions->tLimits.iReserved != 0u || pOptions->tLimits.iFlags != 0u ||
		pOptions->tLimits.iMaxCompressionRatio != 0u ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid TOML options or unsupported resource limits");
		return false;
	}
	*pResolved = *pOptions;
	return true;
}

bool __xrtTomlLimitNode(xrt_toml_context* pContext, uint32 iDepth,
	uint64 iLine, uint64 iColumn)
{
	if ( pContext->Options.tLimits.iMaxDepth != 0u &&
		iDepth > pContext->Options.tLimits.iMaxDepth ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, iLine,
			iColumn, 0u, "TOML nesting depth exceeds the configured limit");
		return false;
	}
	pContext->Nodes += 1u;
	if ( pContext->Options.tLimits.iMaxNodes != 0u &&
		pContext->Nodes > pContext->Options.tLimits.iMaxNodes ) {
		__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT, iLine,
			iColumn, 0u, "TOML node count exceeds the configured limit");
		return false;
	}
	return true;
}

static xvalue* __xrtTomlConvertDateTime(const toml_datum_t* pDatum)
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
		/* 专用值保留本地时间、微秒和时区偏移，不压成单一时间戳。 */
		return xrtTomlCreateOffsetDateTime(pDatum->u.ts.year,
			pDatum->u.ts.month, pDatum->u.ts.day, pDatum->u.ts.hour,
			pDatum->u.ts.minute, pDatum->u.ts.second,
			pDatum->u.ts.usec, pDatum->u.ts.tz);
	default:
		return NULL;
	}
}

xvalue* __xrtTomlConvertDatum(xrt_toml_context* pContext,
	const toml_datum_t* pDatum, uint32 iDepth)
{
	xvalue* pResult = NULL;
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
		return xrtValueString((xstrview){ pDatum->u.str.ptr,
			(size_t)pDatum->u.str.len });
	case TOML_INT64: return xrtValueInt(pDatum->u.int64);
	case TOML_FP64: return xrtValueFloat(pDatum->u.fp64);
	case TOML_BOOLEAN: return xrtValueBool(pDatum->u.boolean);
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
		pResult = xrtValueArray();
		for ( i = 0; pResult != NULL && i < pDatum->u.arr.size; ++i ) {
			xvalue* pItem = __xrtTomlConvertDatum(pContext,
				&pDatum->u.arr.elem[i], iDepth + 1u);
			if ( pItem == NULL || !xrtValueArrayAppendTake(pResult, &pItem) ) {
				xrtValueRelease(pItem);
				xrtValueRelease(pResult);
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
		pResult = xrtValueObject();
		for ( i = 0; pResult != NULL && i < pDatum->u.tab.size; ++i ) {
			xvalue* pItem;
			int iKeySize = pDatum->u.tab.len[i];
			if ( iKeySize < 0 || (pContext->Options.tLimits.iMaxItemBytes != 0u &&
				(uint64)iKeySize > pContext->Options.tLimits.iMaxItemBytes) ) {
				__xrtTomlSetError(pContext->Error, XRT_TOML_ERROR_LIMIT,
					pDatum->u.tab.value[i].lineno, pDatum->u.tab.value[i].colno,
					0u, "TOML key exceeds the configured item limit");
				xrtValueRelease(pResult);
				return NULL;
			}
			pItem = __xrtTomlConvertDatum(pContext, &pDatum->u.tab.value[i],
				iDepth + 1u);
			if ( pItem == NULL || !xrtValueObjectSetTake(pResult,
				(xstrview){ pDatum->u.tab.key[i], (size_t)iKeySize }, &pItem) ) {
				xrtValueRelease(pItem);
				xrtValueRelease(pResult);
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

XRT_API xvalue* xrtTomlParse(xreader* pReader, bool bTakeOwnership,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xrttomlerror tLocalError;
	xrt_toml_context tContext;
	xbuffer* pInput = NULL;
	xvalue* pResult = NULL;
	toml_result_t tParsed;
	uint64 iLimit;
	uint64 iKnownSize = 0u;
	uint64 iPosition = 0u;
	uint32 iParseSize;
	size_t iBadUtf8 = 0u;
	if ( pError == NULL ) { pError = &tLocalError; }
	xrtTomlErrorInit(pError);
	memset(&tContext, 0, sizeof(tContext));
	tContext.Error = pError;
	if ( !__xrtTomlResolveOptions(pOptions, &tContext.Options, pError) ) {
		goto cleanup;
	}
	if ( pReader == NULL ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid TOML reader");
		goto cleanup;
	}
	/* 总大小须减去当前游标；缺少任一查询能力时由 ReadAll 限额。 */
	iLimit = tContext.Options.tLimits.iMaxInputBytes;
	if ( iLimit == 0u || iLimit > INT_MAX ) { iLimit = INT_MAX; }
	xrtClearError();
	if ( xrtReaderSize(pReader, &iKnownSize) &&
		xrtReaderTell(pReader, &iPosition) &&
		iKnownSize > iPosition && iKnownSize - iPosition > iLimit ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML input exceeds the configured limit");
		goto cleanup;
	}
	xrtClearError();
	pInput = xrtReaderReadAll(pReader, (size_t)iLimit);
	if ( pInput == NULL ) {
		const xerror* pCause = xrtGetError();
		xerrkind iKind = pCause != NULL ? xrtErrorKind(pCause) : XERR_IO;
		__xrtTomlSetError(pError,
			iKind == XERR_MEMORY ? XRT_TOML_ERROR_MEMORY :
			(iKind == XERR_RANGE ? XRT_TOML_ERROR_LIMIT : XRT_TOML_ERROR_IO),
			0u, 0u, 0u, "cannot read TOML stream");
		goto cleanup;
	}
	if ( pInput->Size > INT_MAX ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML input exceeds the parser limit");
		goto cleanup;
	}
	if ( !xrtUtf8Valid((xstrview){ (const char*)pInput->Data, pInput->Size },
		&iBadUtf8) ) {
		uint64 iLine = 1u, iColumn = 1u;
		size_t i;
		/* 错误时才扫描前缀；列号按字节计，CRLF 由 LF 计行。 */
		for ( i = 0u; i < iBadUtf8; ++i ) {
			if ( pInput->Data[i] == '\n' ) { iLine++; iColumn = 1u; }
			else { iColumn++; }
		}
		__xrtTomlSetError(pError, XRT_TOML_ERROR_FORMAT,
			iLine, iColumn, iBadUtf8,
			"TOML input is not valid UTF-8");
		goto cleanup;
	}
	iParseSize = (uint32)pInput->Size;
	/* tomlc17 明确要求 len 后还有一个 NUL；流和 Buffer 不承诺该哨兵。 */
	if ( !xrtBufferAppendByte(pInput, 0u) ) {
		__xrtTomlSetError(pError, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot terminate TOML parser input");
		goto cleanup;
	}
	/* 仅观察本次解析器产生的错误，避免调用方旧错误污染分类。 */
	xrtClearError();
	tParsed = toml_parse((const char*)pInput->Data, (int)iParseSize);
	if ( !tParsed.ok ) {
		const xerror* pCause = xrtGetError();
		int iCode = pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
			XRT_TOML_ERROR_MEMORY : XRT_TOML_ERROR_FORMAT;
		__xrtTomlSetError(pError, iCode,
			__xrtTomlErrorLine(tParsed.errmsg), 0u, 0u, "%s",
			tParsed.errmsg[0] != 0 ? tParsed.errmsg : "invalid TOML input");
		toml_free(tParsed);
		goto cleanup;
	}
	pResult = __xrtTomlConvertDatum(&tContext, &tParsed.toptab, 1u);
	toml_free(tParsed);

cleanup:
	xrtBufferDestroy(pInput);
	/* 关闭回调成功也可能清空错误槽；正文失败时保留原始 XRT 首因。 */
	bool bHadFailure = pError->iCode != XRT_TOML_OK;
	xerror* pSaved = bHadFailure ? xrtTakeError() : NULL;
	if ( bTakeOwnership && !xrtReaderDestroy(pReader) &&
		pError->iCode == XRT_TOML_OK ) {
		const xerror* pCause = xrtGetError();
		__xrtTomlSetError(pError,
			xrtErrorKind(pCause) == XERR_MEMORY ?
				XRT_TOML_ERROR_MEMORY : XRT_TOML_ERROR_IO,
			0u, 0u, 0u, "cannot close TOML stream");
		xrtValueRelease(pResult);
		pResult = NULL;
	}
	if ( bHadFailure ) { xrtSetErrorTake(pSaved); }
	return pResult;
}

XRT_API xvalue* xrtTomlParseMemory(const void* pData, uint64 iSize,
	const xrttomloptions* pOptions, xrttomlerror* pError)
{
	xreader* pReader;
	if ( iSize > SIZE_MAX ) {
		xrtTomlErrorInit(pError);
		__xrtTomlSetError(pError, XRT_TOML_ERROR_LIMIT, 0u, 0u, 0u,
			"TOML input size exceeds the platform limit");
		return NULL;
	}
	pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize }) : NULL;
	if ( pReader == NULL && (pData != NULL || iSize == 0u) ) {
		xrtTomlErrorInit(pError);
		__xrtTomlSetError(pError, XRT_TOML_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate TOML reader");
		return NULL;
	}
	return xrtTomlParse(pReader, true, pOptions, pError);
}

XRT_API xvalue* xrtTomlLoad(cstr sPath, const xrttomloptions* pOptions,
	xrttomlerror* pError)
{
	xreader* pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	if ( pReader == NULL && sPath != NULL ) {
		const xerror* pCause = xrtGetError();
		int iCode = pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
			XRT_TOML_ERROR_MEMORY : XRT_TOML_ERROR_IO;
		xrtTomlErrorInit(pError);
		__xrtTomlSetError(pError, iCode, 0u, 0u, 0u,
			"cannot open TOML file");
		return NULL;
	}
	return xrtTomlParse(pReader, true, pOptions, pError);
}
