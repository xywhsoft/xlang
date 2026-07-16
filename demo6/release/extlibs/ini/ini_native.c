#include "include/xrt_ini.h"

#if defined(_WIN32) || defined(_WIN64)
#define XINI_EXPORT __declspec(dllexport)
#else
#define XINI_EXPORT
#endif



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

static void xini_publish_error(const xrtiniconfig* pConfig)
{
	const xrtinierror* pError = xrtIniError(pConfig);
	xrtSetError(xrtFormat("%s", pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "INI operation failed"), TRUE);
}

static void* xini_validate(xrtiniconfig* pConfig)
{
	if ( pConfig == NULL || xrtIniError(pConfig)->iCode != XRT_INI_OK ) {
		xini_publish_error(pConfig);
		xrtIniDestroy(pConfig);
		return NULL;
	}
	return pConfig;
}



XINI_EXPORT void* xini_create(bool bCaseSensitive, bool bStrictDuplicates,
	bool bInterpolation, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	return xini_validate(xrtIniCreate(&tOptions));
}

XINI_EXPORT void* xini_parse(const void* pData, uint64 iSize,
	bool bCaseSensitive, bool bStrictDuplicates, bool bInterpolation,
	uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	return xini_validate(xrtIniParseMemory(pData, iSize, &tOptions));
}

XINI_EXPORT void* xini_load(str sPath, bool bCaseSensitive,
	bool bStrictDuplicates, bool bInterpolation, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxNodes, uint32 iMaxDepth)
{
	xrtinioptions tOptions;
	xrtClearError();
	xini_options(&tOptions, bCaseSensitive, bStrictDuplicates, bInterpolation,
		iMaxInput, iMaxOutput, iMaxItem, iMaxNodes, iMaxDepth);
	return xini_validate(xrtIniLoad(sPath, &tOptions));
}

XINI_EXPORT void xini_destroy(void* pValue)
{
	xrtIniDestroy((xrtiniconfig*)pValue);
}

XINI_EXPORT int xini_error_code(void* pValue)
{
	const xrtinierror* pError = xrtIniError((xrtiniconfig*)pValue);
	return pError != NULL ? pError->iCode : XRT_INI_ERROR_ARGUMENT;
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

XINI_EXPORT int xini_section_count(void* pValue)
{
	return (int)xrtIniSectionCount((xrtiniconfig*)pValue);
}

XINI_EXPORT void* xini_section_name(void* pValue, int iIndex)
{
	const char* sName = iIndex >= 0 ?
		xrtIniSectionName((xrtiniconfig*)pValue, (uint32)iIndex) : NULL;
	if ( sName == NULL ) {
		xrtSetError(xrtFormat("INI section index is out of range"), TRUE);
		return NULL;
	}
	return xrtCopyStr((str)sName, 0u);
}

XINI_EXPORT bool xini_has_section(void* pValue, str sSection)
{
	return xrtIniHasSection((xrtiniconfig*)pValue, sSection);
}

XINI_EXPORT int xini_key_count(void* pValue, str sSection)
{
	return (int)xrtIniKeyCount((xrtiniconfig*)pValue, sSection);
}

XINI_EXPORT void* xini_key_name(void* pValue, str sSection, int iIndex)
{
	const char* sName = iIndex >= 0 ?
		xrtIniKeyName((xrtiniconfig*)pValue, sSection, (uint32)iIndex) : NULL;
	if ( sName == NULL ) {
		xrtSetError(xrtFormat("INI key index is out of range"), TRUE);
		return NULL;
	}
	return xrtCopyStr((str)sName, 0u);
}

XINI_EXPORT bool xini_has(void* pValue, str sSection, str sKey)
{
	return xrtIniHas((xrtiniconfig*)pValue, sSection, sKey);
}

XINI_EXPORT void* xini_get(void* pValue, str sSection, str sKey)
{
	str sValue = xrtIniGet((xrtiniconfig*)pValue, sSection, sKey);
	if ( sValue == NULL ) { xini_publish_error((xrtiniconfig*)pValue); }
	return sValue;
}

XINI_EXPORT int64 xini_get_int(void* pValue, str sSection, str sKey)
{
	int64 iValue = 0;
	if ( !xrtIniGetInt((xrtiniconfig*)pValue, sSection, sKey, &iValue) ) {
		xini_publish_error((xrtiniconfig*)pValue);
	}
	return iValue;
}

XINI_EXPORT double xini_get_float(void* pValue, str sSection, str sKey)
{
	double fValue = 0.0;
	if ( !xrtIniGetFloat((xrtiniconfig*)pValue, sSection, sKey, &fValue) ) {
		xini_publish_error((xrtiniconfig*)pValue);
	}
	return fValue;
}

XINI_EXPORT bool xini_get_bool(void* pValue, str sSection, str sKey)
{
	bool bValue = FALSE;
	if ( !xrtIniGetBool((xrtiniconfig*)pValue, sSection, sKey, &bValue) ) {
		xini_publish_error((xrtiniconfig*)pValue);
	}
	return bValue;
}

XINI_EXPORT bool xini_set(void* pValue, str sSection, str sKey, str sValue)
{
	if ( !xrtIniSet((xrtiniconfig*)pValue, sSection, sKey, sValue) ) {
		xini_publish_error((xrtiniconfig*)pValue);
		return FALSE;
	}
	return TRUE;
}

XINI_EXPORT bool xini_remove(void* pValue, str sSection, str sKey)
{
	return xrtIniRemove((xrtiniconfig*)pValue, sSection, sKey);
}

XINI_EXPORT bool xini_remove_section(void* pValue, str sSection)
{
	return xrtIniRemoveSection((xrtiniconfig*)pValue, sSection);
}

XINI_EXPORT void* xini_stringify(void* pValue)
{
	xbuffer pOutput = xrtIniStringify((xrtiniconfig*)pValue);
	if ( pOutput == NULL ) { xini_publish_error((xrtiniconfig*)pValue); }
	return pOutput;
}

XINI_EXPORT bool xini_save(void* pValue, str sPath)
{
	if ( !xrtIniSave((xrtiniconfig*)pValue, sPath) ) {
		xini_publish_error((xrtiniconfig*)pValue);
		return FALSE;
	}
	return TRUE;
}
