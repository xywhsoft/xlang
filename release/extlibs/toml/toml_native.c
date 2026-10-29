#include "xrt_toml_internal.h"

#if defined(_WIN32) || defined(_WIN64)
#define XTOML_EXPORT __declspec(dllexport)
#else
#define XTOML_EXPORT
#endif



static xonce __xtomlErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xtomlErrorKey = NULL;

static bool __xtomlInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xtomlErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xtomlErrorKey != NULL;
}

static xrttomlerror* __xtomlErrorGet(void)
{
	xrttomlerror* pError;
	if ( !xrtOnce(&__xtomlErrorOnce, __xtomlInitErrorKey, NULL) ||
		__xtomlErrorKey == NULL ) { return NULL; }
	pError = (xrttomlerror*)xrtThreadKeyGet(__xtomlErrorKey);
	if ( pError != NULL ) { return pError; }
	pError = (xrttomlerror*)xrtMalloc(sizeof(*pError));
	if ( pError == NULL || !xrtThreadKeySet(__xtomlErrorKey, pError) ) {
		xrtFree(pError);
		return NULL;
	}
	xrtTomlErrorInit(pError);
	return pError;
}

static void __xtomlPublishError(const xrttomlerror* pError)
{
	int iType = XERR_VALUE;
	if ( pError != NULL ) {
		switch ( pError->iCode ) {
		case XRT_TOML_ERROR_MEMORY: iType = XERR_MEMORY; break;
		case XRT_TOML_ERROR_IO: iType = XERR_IO; break;
		case XRT_TOML_ERROR_LIMIT: iType = XERR_RANGE; break;
		case XRT_TOML_ERROR_TYPE: iType = XERR_TYPE; break;
		default: break;
		}
	}
	xrtSetErrorInfo(iType, "xlang.extlib.toml",
		pError != NULL ? pError->iCode : XRT_TOML_ERROR_MEMORY,
		pError != NULL && pError->sMessage[0] != 0 ? pError->sMessage :
			"TOML operation failed");
}

static void __xtomlOptions(xrttomloptions* pOptions, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth, xrttomlerror* pError)
{
	xrtTomlOptionsInit(pOptions);
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pOptions->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxEntries != 0u ) { pOptions->tLimits.iMaxEntries = iMaxEntries; }
	if ( iMaxNodes != 0u ) { pOptions->tLimits.iMaxNodes = iMaxNodes; }
	if ( iMaxDepth != 0u ) { pOptions->tLimits.iMaxDepth = iMaxDepth; }
	xrtTomlErrorInit(pError);
	xrtClearError();
}

XTOML_EXPORT int64 xtoml_error_code(void)
{
	xrttomlerror* pError = __xtomlErrorGet();
	return pError != NULL ? pError->iCode : XRT_TOML_ERROR_MEMORY;
}
XTOML_EXPORT int64 xtoml_error_line(void)
{
	xrttomlerror* pError = __xtomlErrorGet();
	return pError != NULL ? (int64)pError->iLine : 0;
}
XTOML_EXPORT int64 xtoml_error_column(void)
{
	xrttomlerror* pError = __xtomlErrorGet();
	return pError != NULL ? (int64)pError->iColumn : 0;
}
XTOML_EXPORT int64 xtoml_error_offset(void)
{
	xrttomlerror* pError = __xtomlErrorGet();
	return pError != NULL ? (int64)pError->iOffset : 0;
}
XTOML_EXPORT void* xtoml_error_message(void)
{
	xrttomlerror* pError = __xtomlErrorGet();
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "TOML operation failed";
	return xrtStrDup(sMessage);
}

XTOML_EXPORT xvalue* xtoml_parse(const char* pData, unsigned long long iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xvalue* pResult;
	if ( pError == NULL ) { return xrtValueNull(); }
	__xtomlOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { __xtomlPublishError(pError); }
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT xvalue* xtoml_load(const char* sPath,
	unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned long long iMaxItem, unsigned long long iMaxEntries,
	unsigned long long iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xvalue* pResult;
	if ( pError == NULL ) { return xrtValueNull(); }
	__xtomlOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlLoad(sPath, &tOptions, pError);
	if ( pResult == NULL ) { __xtomlPublishError(pError); }
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT void* xtoml_stringify(xvalue* pValue,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned long long iMaxNodes,
	uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xbuffer* pResult;
	if ( pError == NULL ) { return NULL; }
	__xtomlOptions(&tOptions, 0u, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlStringify(pValue, &tOptions, pError);
	if ( pResult == NULL ) { __xtomlPublishError(pError); }
	return pResult;
}

/* 将文本缓冲直接交给调用方；返回值由 xrtFree 释放。 */
XTOML_EXPORT void* xtoml_stringify_text(xvalue* pValue,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned long long iMaxNodes,
	uint32 iMaxDepth)
{
	xbuffer* pResult = (xbuffer*)xtoml_stringify(pValue, iMaxOutput,
		iMaxItem, iMaxEntries, iMaxNodes, iMaxDepth);
	char* sText;
	if ( pResult == NULL ) { return NULL; }
	if ( !xrtBufferAppendByte(pResult, 0) ) {
		xrtBufferDestroy(pResult);
		return NULL;
	}
	sText = (char*)xrtBufferTake(pResult, NULL, NULL);
	xrtBufferDestroy(pResult);
	return sText;
}

XTOML_EXPORT bool xtoml_save(const char* sPath, xvalue* pValue,
	unsigned long long iMaxOutput, unsigned long long iMaxItem,
	unsigned long long iMaxEntries, unsigned long long iMaxNodes,
	uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	bool bResult;
	if ( pError == NULL ) { return false; }
	__xtomlOptions(&tOptions, 0u, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth, pError);
	bResult = xrtTomlSave(sPath, pValue, &tOptions, pError);
	if ( !bResult ) { __xtomlPublishError(pError); }
	return bResult;
}

XTOML_EXPORT xvalue* xtoml_date(int iYear, int iMonth, int iDay)
{
	xvalue* pResult = xrtTomlCreateDate(iYear, iMonth, iDay);
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT xvalue* xtoml_time(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	xvalue* pResult = xrtTomlCreateTime(iHour, iMinute, iSecond, iMicrosecond);
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT xvalue* xtoml_datetime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond)
{
	xvalue* pResult = xrtTomlCreateDateTime(iYear, iMonth, iDay, iHour,
		iMinute, iSecond, iMicrosecond);
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT xvalue* xtoml_offset_datetime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond, int iOffset)
{
	xvalue* pResult = xrtTomlCreateOffsetDateTime(iYear, iMonth, iDay, iHour,
		iMinute, iSecond, iMicrosecond, iOffset);
	return pResult != NULL ? pResult : xrtValueNull();
}

XTOML_EXPORT int64 xtoml_datetime_kind(xvalue* pValue)
{
	int iKind = 0;
	xrtTomlIsDateTimeValue(pValue, &iKind);
	return iKind;
}

XTOML_EXPORT int64 xtoml_datetime_part(xvalue* pValue, int iPart)
{
	xrttomldatetime Time;
	if ( !xrtTomlGetDateTime(pValue, &Time) ) { return 0; }
	switch ( iPart ) {
	case 0: return Time.iYear;
	case 1: return Time.iMonth;
	case 2: return Time.iDay;
	case 3: return Time.iHour;
	case 4: return Time.iMinute;
	case 5: return Time.iSecond;
	case 6: return Time.iMicrosecond;
	case 7: return Time.iTimezoneMinutes;
	default: return 0;
	}
}

XTOML_EXPORT void* xtoml_datetime_text(xvalue* pValue)
{
	xrttomldatetime Time;
	return xrtTomlGetDateTime(pValue, &Time) ?
		xrtTomlDateTimeToString(&Time) : NULL;
}
