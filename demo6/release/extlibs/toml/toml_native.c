#include "include/xrt_toml.h"

#if defined(_WIN32) || defined(_WIN64)
#define XTOML_EXPORT __declspec(dllexport)
#else
#define XTOML_EXPORT
#endif



static xrttomlerror* __xtomlErrorGet(void)
{
	return (xrttomlerror*)xrtThreadLocalGetOrCreate(
		"xlang.toml.error.v1", sizeof(xrttomlerror));
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

XTOML_EXPORT int xtoml_error_code(void)
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
	return xrtCopyStr((str)sMessage, 0u);
}

XTOML_EXPORT xvalue xtoml_parse(const void* pData, uint64 iSize,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xvalue pResult;
	if ( pError == NULL ) { return xvoCreateNull(); }
	__xtomlOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlParseMemory(pData, iSize, &tOptions, pError);
	return pResult != NULL ? pResult : xvoCreateNull();
}

XTOML_EXPORT xvalue xtoml_load(str sPath, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xvalue pResult;
	if ( pError == NULL ) { return xvoCreateNull(); }
	__xtomlOptions(&tOptions, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlLoad(sPath, &tOptions, pError);
	return pResult != NULL ? pResult : xvoCreateNull();
}

XTOML_EXPORT void* xtoml_stringify(xvalue pValue, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	xbuffer pResult;
	if ( pError == NULL ) { return NULL; }
	__xtomlOptions(&tOptions, 0u, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth, pError);
	pResult = xrtTomlStringify(pValue, &tOptions, pError);
	return pResult;
}

XTOML_EXPORT bool xtoml_save(str sPath, xvalue pValue,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrttomloptions tOptions;
	xrttomlerror* pError = __xtomlErrorGet();
	bool bResult;
	if ( pError == NULL ) { return FALSE; }
	__xtomlOptions(&tOptions, 0u, iMaxOutput, iMaxItem, iMaxEntries,
		iMaxNodes, iMaxDepth, pError);
	bResult = xrtTomlSave(sPath, pValue, &tOptions, pError);
	return bResult;
}

XTOML_EXPORT xvalue xtoml_date(int iYear, int iMonth, int iDay)
{
	return xrtTomlCreateDate(iYear, iMonth, iDay);
}

XTOML_EXPORT xvalue xtoml_time(int iHour, int iMinute, int iSecond,
	int iMicrosecond)
{
	return xrtTomlCreateTime(iHour, iMinute, iSecond, iMicrosecond);
}

XTOML_EXPORT xvalue xtoml_datetime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond)
{
	return xrtTomlCreateDateTime(iYear, iMonth, iDay, iHour, iMinute,
		iSecond, iMicrosecond);
}

XTOML_EXPORT xvalue xtoml_offset_datetime(int iYear, int iMonth, int iDay,
	int iHour, int iMinute, int iSecond, int iMicrosecond, int iOffset)
{
	return xrtTomlCreateOffsetDateTime(iYear, iMonth, iDay, iHour, iMinute,
		iSecond, iMicrosecond, iOffset);
}

XTOML_EXPORT int xtoml_datetime_kind(xvalue pValue)
{
	int iKind = 0;
	xrtTomlIsDateTimeValue(pValue, &iKind);
	return iKind;
}

XTOML_EXPORT int xtoml_datetime_part(xvalue pValue, int iPart)
{
	const xrttomldatetime* pTime = xrtTomlGetDateTime(pValue);
	if ( pTime == NULL ) { return 0; }
	switch ( iPart ) {
	case 0: return pTime->iYear;
	case 1: return pTime->iMonth;
	case 2: return pTime->iDay;
	case 3: return pTime->iHour;
	case 4: return pTime->iMinute;
	case 5: return pTime->iSecond;
	case 6: return pTime->iMicrosecond;
	case 7: return pTime->iTimezoneMinutes;
	default: return 0;
	}
}

XTOML_EXPORT void* xtoml_datetime_text(xvalue pValue)
{
	return xrtTomlDateTimeToString(xrtTomlGetDateTime(pValue));
}
