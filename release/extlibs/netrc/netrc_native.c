#include "xrt_netrc_internal.h"

#if defined(_WIN32) || defined(_WIN64)
#define XNETRC_EXPORT __declspec(dllexport)
#else
#define XNETRC_EXPORT
#endif

static xonce __xnetrcErrorOnce = XRT_ONCE_INIT;
static xthreadkey* __xnetrcErrorKey = NULL;

static bool __xnetrcInitErrorKey(ptr pContext)
{
	(void)pContext;
	__xnetrcErrorKey = xrtThreadKeyCreate(xrtFree);
	return __xnetrcErrorKey != NULL;
}

static xrtnetrcerror* __xnetrcError(void)
{
	xrtnetrcerror* pError;

	if ( !xrtOnce(&__xnetrcErrorOnce, __xnetrcInitErrorKey, NULL) ||
		__xnetrcErrorKey == NULL ) {
		return NULL;
	}
	pError = (xrtnetrcerror*)xrtThreadKeyGet(__xnetrcErrorKey);
	if ( pError != NULL ) { return pError; }
	pError = (xrtnetrcerror*)xrtMalloc(sizeof(*pError));
	if ( pError == NULL || !xrtThreadKeySet(__xnetrcErrorKey, pError) ) {
		xrtFree(pError);
		return NULL;
	}
	xrtNetrcErrorInit(pError);
	return pError;
}

static void __xnetrcClearError(xrtnetrcerror* pError)
{
	xrtClearError();
	if ( pError != NULL ) { xrtNetrcErrorInit(pError); }
}

/* 可预期的输入错误由 XL 包装层抛类型化异常，不抢占 XRT 运行时错误。 */
static void __xnetrcDeferError(const xrtnetrcerror* pError)
{
	if ( pError != NULL && pError->iCode != XRT_NETRC_OK &&
		pError->iCode != XRT_NETRC_ERROR_MEMORY ) { xrtClearError(); }
}

static void __xnetrcOptions(xrtnetrcoptions* pOptions, bool bStrict,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtNetrcOptionsInit(pOptions);
	pOptions->bStrictDuplicates = bStrict;
	pOptions->tLimits.iMaxInputBytes = iMaxInput;
	pOptions->tLimits.iMaxOutputBytes = iMaxOutput;
	pOptions->tLimits.iMaxItemBytes = iMaxItem;
	pOptions->tLimits.iMaxEntries = iMaxEntries;
	pOptions->tLimits.iMaxNodes = iMaxNodes;
}

XNETRC_EXPORT int64 xnetrc_error_code(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	return pError != NULL ? pError->iCode : XRT_NETRC_ERROR_MEMORY;
}

XNETRC_EXPORT int64 xnetrc_error_line(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	return pError != NULL ? (int64)pError->iLine : 0;
}

XNETRC_EXPORT int64 xnetrc_error_column(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	return pError != NULL ? (int64)pError->iColumn : 0;
}

XNETRC_EXPORT int64 xnetrc_error_offset(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	return pError != NULL ? (int64)pError->iOffset : 0;
}

XNETRC_EXPORT char* xnetrc_error_message(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "netrc operation failed";
	return xrtStrDup(sMessage);
}

XNETRC_EXPORT xvalue* xnetrc_parse(const char* pData, uint64 iSize,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	xvalue* pResult;
	if ( pError == NULL ) { return xrtValueNull(); }
	__xnetrcOptions(&tOptions, bStrict, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	__xnetrcClearError(pError);
	pResult = xrtNetrcParseMemory(pData, iSize, &tOptions, pError);
	if ( pResult == NULL ) { __xnetrcDeferError(pError); }
	return pResult != NULL ? pResult : xrtValueNull();
}

XNETRC_EXPORT xvalue* xnetrc_load(cstr sPath, bool bStrict,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	xvalue* pResult;
	if ( pError == NULL ) { return xrtValueNull(); }
	__xnetrcOptions(&tOptions, bStrict, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	__xnetrcClearError(pError);
	pResult = xrtNetrcLoad((str)sPath, &tOptions, pError);
	if ( pResult == NULL ) { __xnetrcDeferError(pError); }
	return pResult != NULL ? pResult : xrtValueNull();
}

XNETRC_EXPORT void* xnetrc_stringify(xvalue* pValue, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	if ( pError == NULL ) { return NULL; }
	__xnetrcOptions(&tOptions, true, 0u, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	__xnetrcClearError(pError);
	{
		xbuffer* pResult = xrtNetrcStringify(pValue, &tOptions, pError);
		if ( pResult == NULL ) { __xnetrcDeferError(pError); }
		return pResult;
	}
}

/* 返回独立的 XRT 文本分配，调用方以 xrtFree 释放。 */
XNETRC_EXPORT void* xnetrc_stringify_text(xvalue* pValue, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes)
{
	xbuffer* pResult = (xbuffer*)xnetrc_stringify(pValue, iMaxOutput,
		iMaxItem, iMaxEntries, iMaxNodes);
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

XNETRC_EXPORT bool xnetrc_save(cstr sPath, xvalue* pValue,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	if ( pError == NULL ) { return false; }
	__xnetrcOptions(&tOptions, true, 0u, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	__xnetrcClearError(pError);
	if ( !xrtNetrcSave((str)sPath, pValue, &tOptions, pError) ) {
		__xnetrcDeferError(pError);
		return false;
	}
	return true;
}
