#include "xrt_ini_internal.h"

#if defined(_WIN32) || defined(_WIN64)
#define XINI_EXPORT __declspec(dllexport)
#else
#define XINI_EXPORT
#endif

/* XRT INI 的 str 入参只读；此处对齐 xlang string 的 const char* ABI。 */
static str xini_borrowed(cstr sValue)
{
	return (str)sValue;
}

/* 格式错误交给 ConfigError；内存错误仍由 XRT 运行时直接传播。 */
static void xini_defer_error(const xrtiniconfig* pConfig)
{
	const xrtinierror* pError = xrtIniError(pConfig);
	if ( pError != NULL && pError->iCode != XRT_INI_OK &&
		pError->iCode != XRT_INI_ERROR_MEMORY ) { xrtClearError(); }
}



static void xini_options(xrtinioptions* pOptions, bool bCaseSensitive,
	bool bStrictDuplicates, bool bInterpolation, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtIniOptionsInit(pOptions);
	pOptions->bCaseSensitive = bCaseSensitive;
	pOptions->bStrictDuplicates = bStrictDuplicates;
	pOptions->bInterpolation = bInterpolation;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pOptions->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxNodes != 0u ) { pOptions->tLimits.iMaxNodes = iMaxNodes; }
	if ( iMaxDepth != 0u ) { pOptions->tLimits.iMaxDepth = iMaxDepth; }
}

XINI_EXPORT void* xini_create(bool bCaseSensitive, bool bStrictDuplicates,
	bool bInterpolation, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	pConfig = xrtIniCreate(&tOptions);
	xini_defer_error(pConfig);
	return pConfig;
}

XINI_EXPORT void* xini_parse(const char* pData, uint64 iSize,
	bool bCaseSensitive, bool bStrictDuplicates, bool bInterpolation,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	pConfig = xrtIniParseMemory(pData, iSize, &tOptions);
	xini_defer_error(pConfig);
	return pConfig;
}

XINI_EXPORT void* xini_load(cstr sPath, bool bCaseSensitive,
	bool bStrictDuplicates, bool bInterpolation, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	pConfig = xrtIniLoad(xini_borrowed(sPath), &tOptions);
	xini_defer_error(pConfig);
	return pConfig;
}

XINI_EXPORT void xini_destroy(void* pValue)
{
	xrtIniDestroy((xrtiniconfig*)pValue);
}

XINI_EXPORT int64 xini_error_code(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return pError != NULL ? pError->iCode : XRT_INI_ERROR_ARGUMENT;
}

XINI_EXPORT void* xini_error_text(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return xrtStrDup(pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "INI operation failed");
}

XINI_EXPORT int64 xini_error_line(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return pError != NULL ? (int64)pError->iLine : 0;
}

XINI_EXPORT int64 xini_error_column(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return pError != NULL ? (int64)pError->iColumn : 0;
}

XINI_EXPORT int64 xini_error_offset(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return pError != NULL ? (int64)pError->iOffset : 0;
}

XINI_EXPORT int64 xini_section_count(void* pValue)
{
	return (int64)xrtIniSectionCount((xrtiniconfig*)pValue);
}

XINI_EXPORT void* xini_section_name(void* pValue, int iIndex)
{
	const char* sName = iIndex >= 0 ?
		xrtIniSectionName((xrtiniconfig*)pValue, (uint32)iIndex) : NULL;
	return sName != NULL ? xrtStrDup(sName) : NULL;
}

XINI_EXPORT bool xini_has_section(void* pValue, cstr sSection)
{
	return xrtIniHasSection((xrtiniconfig*)pValue, xini_borrowed(sSection));
}

XINI_EXPORT int64 xini_key_count(void* pValue, cstr sSection)
{
	return (int64)xrtIniKeyCount((xrtiniconfig*)pValue, xini_borrowed(sSection));
}

XINI_EXPORT void* xini_key_name(void* pValue, cstr sSection, int iIndex)
{
	const char* sName = iIndex >= 0 ?
		xrtIniKeyName((xrtiniconfig*)pValue, xini_borrowed(sSection), (uint32)iIndex) : NULL;
	return sName != NULL ? xrtStrDup(sName) : NULL;
}

XINI_EXPORT bool xini_has(void* pValue, cstr sSection, cstr sKey)
{
	return xrtIniHas((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey));
}

XINI_EXPORT void* xini_get(void* pValue, cstr sSection, cstr sKey)
{
	str sValue = xrtIniGet((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey));
	xini_defer_error((xrtiniconfig*)pValue);
	return sValue;
}

XINI_EXPORT int64 xini_get_int(void* pValue, cstr sSection, cstr sKey)
{
	int64 iValue = 0;
	xrtIniGetInt((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey), &iValue);
	xini_defer_error((xrtiniconfig*)pValue);
	return iValue;
}

XINI_EXPORT double xini_get_float(void* pValue, cstr sSection, cstr sKey)
{
	double fValue = 0.0;
	xrtIniGetFloat((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey), &fValue);
	xini_defer_error((xrtiniconfig*)pValue);
	return fValue;
}

XINI_EXPORT bool xini_get_bool(void* pValue, cstr sSection, cstr sKey)
{
	bool bValue = false;
	xrtIniGetBool((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey), &bValue);
	xini_defer_error((xrtiniconfig*)pValue);
	return bValue;
}

XINI_EXPORT bool xini_set(void* pValue, cstr sSection, cstr sKey, cstr sValue)
{
	bool bResult = xrtIniSet((xrtiniconfig*)pValue, xini_borrowed(sSection),
		xini_borrowed(sKey), xini_borrowed(sValue));
	xini_defer_error((xrtiniconfig*)pValue);
	return bResult;
}

XINI_EXPORT bool xini_remove(void* pValue, cstr sSection, cstr sKey)
{
	return xrtIniRemove((xrtiniconfig*)pValue, xini_borrowed(sSection), xini_borrowed(sKey));
}

XINI_EXPORT bool xini_remove_section(void* pValue, cstr sSection)
{
	return xrtIniRemoveSection((xrtiniconfig*)pValue, xini_borrowed(sSection));
}

XINI_EXPORT void* xini_stringify(void* pValue)
{
	xbuffer* pOutput = xrtIniStringify((xrtiniconfig*)pValue);
	xini_defer_error((xrtiniconfig*)pValue);
	return pOutput;
}

XINI_EXPORT bool xini_save(void* pValue, cstr sPath)
{
	bool bResult = xrtIniSave((xrtiniconfig*)pValue, xini_borrowed(sPath));
	xini_defer_error((xrtiniconfig*)pValue);
	return bResult;
}
