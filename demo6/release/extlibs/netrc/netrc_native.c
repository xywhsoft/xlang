#include "include/xrt_netrc.h"

#if defined(_WIN32) || defined(_WIN64)
#define XNETRC_EXPORT __declspec(dllexport)
#else
#define XNETRC_EXPORT
#endif

static xrtnetrcerror* __xnetrcError(void)
{
	return (xrtnetrcerror*)xrtThreadLocalGetOrCreate(
		"xlang.netrc.error.v1", sizeof(xrtnetrcerror));
}

static void __xnetrcOptions(xrtnetrcoptions* pOptions, bool bStrict,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtNetrcOptionsInit(pOptions);
	pOptions->bStrictDuplicates = bStrict;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pOptions->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxEntries != 0u ) { pOptions->tLimits.iMaxEntries = iMaxEntries; }
	if ( iMaxNodes != 0u ) { pOptions->tLimits.iMaxNodes = iMaxNodes; }
}

XNETRC_EXPORT int xnetrc_error_code(void)
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

XNETRC_EXPORT void* xnetrc_error_message(void)
{
	xrtnetrcerror* pError = __xnetrcError();
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "netrc operation failed";
	return xrtCopyStr((str)sMessage, 0u);
}

XNETRC_EXPORT xvalue xnetrc_parse(const void* pData, uint64 iSize,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	xvalue pResult;
	if ( pError == NULL ) { return xvoCreateNull(); }
	__xnetrcOptions(&tOptions, bStrict, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	pResult = xrtNetrcParseMemory(pData, iSize, &tOptions, pError);
	return pResult != NULL ? pResult : xvoCreateNull();
}

XNETRC_EXPORT xvalue xnetrc_load(str sPath, bool bStrict,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	xvalue pResult;
	if ( pError == NULL ) { return xvoCreateNull(); }
	__xnetrcOptions(&tOptions, bStrict, iMaxInput, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	pResult = xrtNetrcLoad(sPath, &tOptions, pError);
	return pResult != NULL ? pResult : xvoCreateNull();
}

XNETRC_EXPORT void* xnetrc_stringify(xvalue pValue, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	if ( pError == NULL ) { return NULL; }
	__xnetrcOptions(&tOptions, TRUE, 0u, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	return xrtNetrcStringify(pValue, &tOptions, pError);
}

XNETRC_EXPORT bool xnetrc_save(str sPath, xvalue pValue,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxEntries, uint64 iMaxNodes)
{
	xrtnetrcoptions tOptions;
	xrtnetrcerror* pError = __xnetrcError();
	if ( pError == NULL ) { return FALSE; }
	__xnetrcOptions(&tOptions, TRUE, 0u, iMaxOutput, iMaxItem,
		iMaxEntries, iMaxNodes);
	return xrtNetrcSave(sPath, pValue, &tOptions, pError);
}
