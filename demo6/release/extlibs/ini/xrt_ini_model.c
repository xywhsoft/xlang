#include "xrt_ini_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



XRT_API void xrtIniOptionsInit(xrtinioptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_INI_OPTIONS_VERSION;
	pOptions->bCaseSensitive = true;
	pOptions->bStrictDuplicates = true;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxCompressionRatio = 0u;
}

XRT_API void xrtIniErrorInit(xrtinierror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

bool __xrtIniCopyOptions(xrtinioptions* pOutput, const xrtinioptions* pInput)
{
	xrtIniOptionsInit(pOutput);
	if ( pInput == NULL ) { return true; }
	if ( pInput->iVersion != XRT_INI_OPTIONS_VERSION ||
		pInput->iSize < offsetof(xrtinioptions, tLimits) + sizeof(xrtresourcelimits) ||
		pInput->bReserved ||
		pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pInput->tLimits.iSize < sizeof(xrtresourcelimits) ||
		pInput->tLimits.iReserved != 0u || pInput->tLimits.iFlags != 0u ||
		pInput->tLimits.iMaxCompressionRatio != 0u ) {
		return false;
	}
	memcpy(pOutput, pInput, pInput->iSize < sizeof(*pOutput) ?
		pInput->iSize : sizeof(*pOutput));
	pOutput->iSize = sizeof(*pOutput);
	pOutput->iVersion = XRT_INI_OPTIONS_VERSION;
	return true;
}

void __xrtIniSetError(xrtiniconfig* pConfig, int iCode, uint64 iLine,
	uint64 iColumn, uint64 iOffset, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pConfig == NULL ) { return; }
	pConfig->Error.iCode = iCode;
	pConfig->Error.iLine = iLine;
	pConfig->Error.iColumn = iColumn;
	pConfig->Error.iOffset = iOffset;
	va_start(tArgs, sFormat);
	vsnprintf(pConfig->Error.sMessage, sizeof(pConfig->Error.sMessage), sFormat, tArgs);
	va_end(tArgs);
	pConfig->Error.sMessage[sizeof(pConfig->Error.sMessage) - 1u] = 0;
	pConfig->Failed = true;
	/* 配置对象保存精确位置，XRT 错误槽保存可传播的失败原因。 */
	xrtSetErrorInfo(iCode == XRT_INI_ERROR_MEMORY ? XERR_MEMORY : XERR_STATE,
		"xlang.extlib.ini", iCode,
		pConfig->Error.sMessage);
}

static void __xrtIniClearError(xrtiniconfig* pConfig)
{
	if ( pConfig == NULL ) { return; }
	xrtIniErrorInit(&pConfig->Error);
	pConfig->Failed = false;
	xrtClearError();
}

bool __xrtIniNameEqual(const xrtiniconfig* pConfig, const char* sLeft, const char* sRight)
{
	if ( sLeft == NULL || sRight == NULL ) { return sLeft == sRight; }
	return pConfig != NULL && !pConfig->Options.bCaseSensitive ?
		xrtStrCaseEqual((xstrview){ sLeft, strlen(sLeft) },
			(xstrview){ sRight, strlen(sRight) }) : strcmp(sLeft, sRight) == 0;
}

xrt_ini_section* __xrtIniFindSection(const xrtiniconfig* pConfig, const char* sName)
{
	uint32 i;
	if ( pConfig == NULL || sName == NULL ) { return NULL; }
	for ( i = 0u; i < pConfig->Count; ++i ) {
		if ( __xrtIniNameEqual(pConfig, (const char*)pConfig->Sections[i].Name, sName) ) {
			return &((xrtiniconfig*)pConfig)->Sections[i];
		}
	}
	return NULL;
}

xrt_ini_entry* __xrtIniFindEntry(const xrtiniconfig* pConfig,
	const xrt_ini_section* pSection, const char* sKey)
{
	uint32 i;
	if ( pConfig == NULL || pSection == NULL || sKey == NULL ) { return NULL; }
	for ( i = 0u; i < pSection->Count; ++i ) {
		if ( __xrtIniNameEqual(pConfig, (const char*)pSection->Entries[i].Key, sKey) ) {
			return &((xrt_ini_section*)pSection)->Entries[i];
		}
	}
	return NULL;
}

static bool __xrtIniCanAddNode(xrtiniconfig* pConfig)
{
	if ( pConfig->Options.tLimits.iMaxNodes != 0u &&
		pConfig->Nodes >= pConfig->Options.tLimits.iMaxNodes ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"INI node count exceeds the configured limit");
		return false;
	}
	return true;
}

xrt_ini_section* __xrtIniAddSection(xrtiniconfig* pConfig, const char* sName,
	const char* sLeading, uint64 iLine, bool bFromParser)
{
	xrt_ini_section* pExisting;
	xrt_ini_section* pSections;
	xrt_ini_section* pSection;
	uint32 iCapacity;
	if ( pConfig == NULL || sName == NULL || !xrtUtf8Valid(
		(xstrview){ sName, strlen(sName) }, NULL) ) {
		if ( pConfig != NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_ARGUMENT, iLine, 1u, 0u,
				"invalid INI section name");
		}
		return NULL;
	}
	pExisting = __xrtIniFindSection(pConfig, sName);
	if ( pExisting != NULL ) {
		if ( bFromParser && pConfig->Options.bStrictDuplicates ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_DUPLICATE, iLine, 1u, 0u,
				"duplicate INI section: %s", sName);
			return NULL;
		}
		return pExisting;
	}
	if ( !__xrtIniCanAddNode(pConfig) ) { return NULL; }
	if ( pConfig->Count == pConfig->Capacity ) {
		iCapacity = pConfig->Capacity != 0u ? pConfig->Capacity * 2u : 8u;
		if ( iCapacity < pConfig->Capacity ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, 0u,
				"too many INI sections");
			return NULL;
		}
		pSections = (xrt_ini_section*)xrtRealloc(pConfig->Sections,
			(size_t)iCapacity * sizeof(*pSections));
		if ( pSections == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
				"cannot grow INI section table");
			return NULL;
		}
		pConfig->Sections = pSections;
		pConfig->Capacity = iCapacity;
	}
	pSection = &pConfig->Sections[pConfig->Count];
	memset(pSection, 0, sizeof(*pSection));
	pSection->Name = xrtStrDup(sName);
	pSection->Leading = xrtStrDup(sLeading != NULL ? sLeading : "");
	if ( pSection->Name == NULL || pSection->Leading == NULL ) {
		xrtFree(pSection->Name);
		xrtFree(pSection->Leading);
		memset(pSection, 0, sizeof(*pSection));
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
			"cannot copy INI section");
		return NULL;
	}
	pConfig->Count += 1u;
	pConfig->Nodes += 1u;
	return pSection;
}

xrt_ini_entry* __xrtIniAddEntry(xrtiniconfig* pConfig, xrt_ini_section* pSection,
	const char* sKey, const char* sValue, const char* sLeading,
	uint64 iLine, bool bFromParser)
{
	xrt_ini_entry* pEntry;
	xrt_ini_entry* pEntries;
	str sCopy;
	uint32 iCapacity;
	if ( pConfig == NULL || pSection == NULL || sKey == NULL || sValue == NULL ||
		sKey[0] == 0 || !xrtUtf8Valid((xstrview){ sKey, strlen(sKey) }, NULL) ||
		!xrtUtf8Valid((xstrview){ sValue, strlen(sValue) }, NULL) ) {
		if ( pConfig != NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_ARGUMENT, iLine, 1u, 0u,
				"invalid INI key or value");
		}
		return NULL;
	}
	pEntry = __xrtIniFindEntry(pConfig, pSection, sKey);
	if ( pEntry != NULL ) {
		if ( bFromParser && pConfig->Options.bStrictDuplicates ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_DUPLICATE, iLine, 1u, 0u,
				"duplicate INI key: %s", sKey);
			return NULL;
		}
		sCopy = xrtStrDup(sValue);
		if ( sCopy == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
				"cannot copy INI value");
			return NULL;
		}
		xrtFree(pEntry->Value);
		pEntry->Value = sCopy;
		return pEntry;
	}
	if ( !__xrtIniCanAddNode(pConfig) ) { return NULL; }
	if ( pConfig->Options.tLimits.iMaxEntries != 0u &&
		pConfig->Nodes - pConfig->Count >= pConfig->Options.tLimits.iMaxEntries ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, 0u,
			"INI key count exceeds the configured limit");
		return NULL;
	}
	if ( pSection->Count == pSection->Capacity ) {
		iCapacity = pSection->Capacity != 0u ? pSection->Capacity * 2u : 8u;
		if ( iCapacity < pSection->Capacity ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, 0u,
				"too many INI keys");
			return NULL;
		}
		pEntries = (xrt_ini_entry*)xrtRealloc(pSection->Entries,
			(size_t)iCapacity * sizeof(*pEntries));
		if ( pEntries == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
				"cannot grow INI key table");
			return NULL;
		}
		pSection->Entries = pEntries;
		pSection->Capacity = iCapacity;
	}
	pEntry = &pSection->Entries[pSection->Count];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Key = xrtStrDup(sKey);
	pEntry->Value = xrtStrDup(sValue);
	pEntry->Leading = xrtStrDup(sLeading != NULL ? sLeading : "");
	pEntry->Line = iLine;
	if ( pEntry->Key == NULL || pEntry->Value == NULL || pEntry->Leading == NULL ) {
		xrtFree(pEntry->Key);
		xrtFree(pEntry->Value);
		xrtFree(pEntry->Leading);
		memset(pEntry, 0, sizeof(*pEntry));
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
			"cannot copy INI entry");
		return NULL;
	}
	pSection->Count += 1u;
	pConfig->Nodes += 1u;
	return pEntry;
}



XRT_API xrtiniconfig* xrtIniCreate(const xrtinioptions* pOptions)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)xrtCalloc(1u, sizeof(*pConfig));
	if ( pConfig == NULL ) { return NULL; }
	xrtIniErrorInit(&pConfig->Error);
	if ( !__xrtIniCopyOptions(&pConfig->Options, pOptions) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_ARGUMENT, 0u, 0u, 0u,
			"invalid INI options");
	}
	return pConfig;
}

XRT_API const xrtinierror* xrtIniError(const xrtiniconfig* pConfig)
{
	return pConfig != NULL ? &pConfig->Error : NULL;
}

XRT_API void xrtIniDestroy(xrtiniconfig* pConfig)
{
	uint32 i;
	uint32 j;
	if ( pConfig == NULL ) { return; }
	for ( i = 0u; i < pConfig->Count; ++i ) {
		xrt_ini_section* pSection = &pConfig->Sections[i];
		for ( j = 0u; j < pSection->Count; ++j ) {
			xrtFree(pSection->Entries[j].Key);
			xrtFree(pSection->Entries[j].Value);
			xrtFree(pSection->Entries[j].Leading);
		}
		xrtFree(pSection->Entries);
		xrtFree(pSection->Name);
		xrtFree(pSection->Leading);
	}
	xrtFree(pConfig->Sections);
	xrtFree(pConfig->Trailing);
	xrtFree(pConfig);
}

XRT_API uint32 xrtIniSectionCount(const xrtiniconfig* pConfig)
{
	return pConfig != NULL ? pConfig->Count : 0u;
}

XRT_API const char* xrtIniSectionName(const xrtiniconfig* pConfig, uint32 iIndex)
{
	return pConfig != NULL && iIndex < pConfig->Count ?
		(const char*)pConfig->Sections[iIndex].Name : NULL;
}

XRT_API bool xrtIniHasSection(const xrtiniconfig* pConfig, str sSection)
{
	return __xrtIniFindSection(pConfig, (const char*)(sSection != NULL ? sSection : (str)"")) != NULL;
}

XRT_API uint32 xrtIniKeyCount(const xrtiniconfig* pConfig, str sSection)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return pSection != NULL ? pSection->Count : 0u;
}

XRT_API const char* xrtIniKeyName(const xrtiniconfig* pConfig, str sSection, uint32 iIndex)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return pSection != NULL && iIndex < pSection->Count ?
		(const char*)pSection->Entries[iIndex].Key : NULL;
}

XRT_API bool xrtIniHas(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return __xrtIniFindEntry(pConfig, pSection, (const char*)sKey) != NULL;
}

XRT_API const char* xrtIniGetRaw(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	xrt_ini_entry* pEntry = __xrtIniFindEntry(pConfig, pSection, (const char*)sKey);
	return pEntry != NULL ? (const char*)pEntry->Value : NULL;
}



/* 展开期间逐段检查，不能先分配超限结果，再在最外层拒绝。 */
static bool __xrtIniAppendResolvedText(xrtiniconfig* pConfig, xbuffer* pOutput,
	const char* pData, size_t iSize)
{
	uint64 iLimit = pConfig->Options.tLimits.iMaxOutputBytes;
	if ( iLimit != 0u && ((uint64)iSize > iLimit ||
		(uint64)pOutput->Size > iLimit - (uint64)iSize) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"resolved INI value exceeds the configured limit");
		return false;
	}
	if ( !xrtBufferAppend(pOutput, (xbytesview){ (cbytes)pData, iSize }) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot grow INI interpolation output");
		return false;
	}
	return true;
}

static bool __xrtIniAppendResolved(xrtiniconfig* pConfig, xbuffer* pOutput,
	const char* sSection, const char* sKey, const char** pSections,
	const char** pKeys, uint32 iDepth, uint32 iMaxDepth)
{
	const char* sValue;
	const char* p;
	const char* pStart;
	uint32 i;
	if ( iDepth >= iMaxDepth ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_INTERPOLATION, 0u, 0u, 0u,
			"INI interpolation exceeds the configured depth");
		return false;
	}
	for ( i = 0u; i < iDepth; ++i ) {
		if ( __xrtIniNameEqual(pConfig, pSections[i], sSection) &&
			__xrtIniNameEqual(pConfig, pKeys[i], sKey) ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_INTERPOLATION, 0u, 0u, 0u,
				"INI interpolation cycle at %s.%s", sSection, sKey);
			return false;
		}
	}
	sValue = xrtIniGetRaw(pConfig, (str)sSection, (str)sKey);
	if ( sValue == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_NOT_FOUND, 0u, 0u, 0u,
			"INI key does not exist: %s.%s", sSection, sKey);
		return false;
	}
	pSections[iDepth] = sSection;
	pKeys[iDepth] = sKey;
	p = sValue;
	pStart = p;
	while ( *p != 0 ) {
		if ( p[0] == '$' && p[1] == '{' ) {
			const char* pClose = strchr(p + 2, '}');
			const char* pDot;
			str sRefSection;
			str sRefKey;
			if ( pClose == NULL || pClose == p + 2 ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_INTERPOLATION, 0u, 0u, 0u,
					"invalid INI interpolation expression");
				return false;
			}
			if ( p != pStart && !__xrtIniAppendResolvedText(pConfig, pOutput,
				pStart, (size_t)(p - pStart)) ) {
				return false;
			}
			pDot = memchr(p + 2, '.', (size_t)(pClose - (p + 2)));
			if ( pDot != NULL ) {
				sRefSection = xrtStrDupN(p + 2, (size_t)(pDot - (p + 2)));
				sRefKey = xrtStrDupN(pDot + 1, (size_t)(pClose - (pDot + 1)));
			} else {
				sRefSection = xrtStrDup(sSection);
				sRefKey = xrtStrDupN(p + 2, (size_t)(pClose - (p + 2)));
			}
			if ( sRefSection == NULL || sRefKey == NULL || sRefKey[0] == 0 ||
				!__xrtIniAppendResolved(pConfig, pOutput, (const char*)sRefSection,
					(const char*)sRefKey, pSections, pKeys, iDepth + 1u, iMaxDepth) ) {
				xrtFree(sRefSection);
				xrtFree(sRefKey);
				if ( !pConfig->Failed ) {
					__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
						"cannot resolve INI interpolation");
				}
				return false;
			}
			xrtFree(sRefSection);
			xrtFree(sRefKey);
			p = pClose + 1;
			pStart = p;
			continue;
		}
		++p;
	}
	if ( p != pStart && !__xrtIniAppendResolvedText(pConfig, pOutput,
		pStart, (size_t)(p - pStart)) ) {
		return false;
	}
	return true;
}

XRT_API str xrtIniGet(const xrtiniconfig* pConst, str sSection, str sKey)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	const char* sRaw;
	xbuffer* pOutput;
	const char** pStack;
	const char** pSections;
	const char** pKeys;
	uint32 iMaxDepth;
	str sResult;
	if ( pConfig == NULL || sKey == NULL ) { return NULL; }
	__xrtIniClearError(pConfig);
	sSection = sSection != NULL ? sSection : (str)"";
	sRaw = xrtIniGetRaw(pConfig, sSection, sKey);
	if ( sRaw == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_NOT_FOUND, 0u, 0u, 0u,
			"INI key does not exist: %s.%s", sSection, sKey);
		return NULL;
	}
	if ( !pConfig->Options.bInterpolation || strstr(sRaw, "${") == NULL ) {
		size_t iSize = strlen(sRaw);
		if ( pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
			(uint64)iSize > pConfig->Options.tLimits.iMaxOutputBytes ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
				"INI value exceeds the configured limit");
			return NULL;
		}
		sResult = xrtStrDupN(sRaw, iSize);
		if ( sResult == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
				"cannot copy INI value");
		}
		return sResult;
	}
	iMaxDepth = pConfig->Options.tLimits.iMaxDepth != 0u ?
		pConfig->Options.tLimits.iMaxDepth : 64u;
	pStack = (const char**)xrtMalloc((size_t)iMaxDepth * sizeof(char*) * 2u);
	pOutput = xrtBufferCreate();
	if ( pStack == NULL || pOutput == NULL || !xrtBufferReserve(pOutput, 256u) ) {
		xrtFree(pStack);
		xrtBufferDestroy(pOutput);
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI interpolation state");
		return NULL;
	}
	pSections = pStack;
	pKeys = pStack + iMaxDepth;
	if ( !__xrtIniAppendResolved(pConfig, pOutput, (const char*)sSection,
		(const char*)sKey, pSections, pKeys, 0u, iMaxDepth) ) {
		xrtFree(pStack);
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	sResult = pOutput->Size != 0u ?
		xrtStrDupN((cstr)pOutput->Data, pOutput->Size) : xrtStrDup("");
	xrtFree(pStack);
	xrtBufferDestroy(pOutput);
	if ( sResult == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot copy resolved INI value");
	}
	return sResult;
}



static str __xrtIniValueForConversion(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	return xrtIniGet(pConfig, sSection, sKey);
}

XRT_API bool xrtIniGetInt(const xrtiniconfig* pConst, str sSection, str sKey, int64* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	int64 iValue;
	if ( pConfig == NULL || pValue == NULL ) { return false; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return false; }
	if ( !xrtIntParse((xstrview){ sValue, strlen(sValue) },
		10u, 0u, &iValue) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid int: %s", sValue);
		xrtFree(sValue);
		return false;
	}
	*pValue = iValue;
	xrtFree(sValue);
	return true;
}

XRT_API bool xrtIniGetFloat(const xrtiniconfig* pConst, str sSection, str sKey, double* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	double fValue;
	if ( pConfig == NULL || pValue == NULL ) { return false; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return false; }
	if ( !xrtNumParse((xstrview){ sValue, strlen(sValue) }, 0u, &fValue) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid float: %s", sValue);
		xrtFree(sValue);
		return false;
	}
	*pValue = fValue;
	xrtFree(sValue);
	return true;
}

XRT_API bool xrtIniGetBool(const xrtiniconfig* pConst, str sSection, str sKey, bool* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	if ( pConfig == NULL || pValue == NULL ) { return false; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return false; }
	if ( xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("true")) ||
		xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("yes")) ||
		xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("on")) || strcmp((char*)sValue, "1") == 0 ) {
		*pValue = true;
	} else if ( xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("false")) ||
		xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("no")) ||
		xrtStrCaseEqual(xrtStrView(sValue), XRT_STR_LITERAL("off")) || strcmp((char*)sValue, "0") == 0 ) {
		*pValue = false;
	} else {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid bool: %s", sValue);
		xrtFree(sValue);
		return false;
	}
	xrtFree(sValue);
	return true;
}



XRT_API bool xrtIniSet(xrtiniconfig* pConfig, str sSection, str sKey, str sValue)
{
	xrt_ini_section* pSection;
	bool bNewSection;
	if ( pConfig == NULL || sKey == NULL || sValue == NULL ) { return false; }
	__xrtIniClearError(pConfig);
	sSection = sSection != NULL ? sSection : (str)"";
	pSection = __xrtIniFindSection(pConfig, (const char*)sSection);
	bNewSection = pSection == NULL;
	if ( pSection == NULL ) {
		pSection = __xrtIniAddSection(pConfig, (const char*)sSection, "", 0u, false);
	}
	if ( pSection == NULL ) { return false; }
	if ( __xrtIniAddEntry(pConfig, pSection,
		(const char*)sKey, (const char*)sValue, "", 0u, false) ) { return true; }
	/* 新节尚未对调用方可见，写键失败时只撤销本次新增的节。 */
	if ( bNewSection ) {
		xrtFree(pSection->Entries);
		xrtFree(pSection->Name);
		xrtFree(pSection->Leading);
		memset(pSection, 0, sizeof(*pSection));
		pConfig->Count -= 1u;
		pConfig->Nodes -= 1u;
	}
	return false;
}

XRT_API bool xrtIniRemove(xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection;
	uint32 i;
	if ( pConfig == NULL || sKey == NULL ) { return false; }
	__xrtIniClearError(pConfig);
	pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	if ( pSection == NULL ) { return false; }
	for ( i = 0u; i < pSection->Count; ++i ) {
		if ( __xrtIniNameEqual(pConfig, (const char*)pSection->Entries[i].Key, (const char*)sKey) ) {
			xrtFree(pSection->Entries[i].Key);
			xrtFree(pSection->Entries[i].Value);
			xrtFree(pSection->Entries[i].Leading);
			memmove(&pSection->Entries[i], &pSection->Entries[i + 1u],
				(size_t)(pSection->Count - i - 1u) * sizeof(*pSection->Entries));
			pSection->Count -= 1u;
			pConfig->Nodes -= 1u;
			return true;
		}
	}
	return false;
}

XRT_API bool xrtIniRemoveSection(xrtiniconfig* pConfig, str sSection)
{
	uint32 i;
	uint32 j;
	if ( pConfig == NULL || sSection == NULL ) { return false; }
	__xrtIniClearError(pConfig);
	for ( i = 0u; i < pConfig->Count; ++i ) {
		if ( __xrtIniNameEqual(pConfig, (const char*)pConfig->Sections[i].Name, (const char*)sSection) ) {
			xrt_ini_section* pSection = &pConfig->Sections[i];
			for ( j = 0u; j < pSection->Count; ++j ) {
				xrtFree(pSection->Entries[j].Key);
				xrtFree(pSection->Entries[j].Value);
				xrtFree(pSection->Entries[j].Leading);
			}
			xrtFree(pSection->Entries);
			xrtFree(pSection->Name);
			xrtFree(pSection->Leading);
			pConfig->Nodes -= (uint64)pSection->Count + 1u;
			memmove(&pConfig->Sections[i], &pConfig->Sections[i + 1u],
				(size_t)(pConfig->Count - i - 1u) * sizeof(*pConfig->Sections));
			pConfig->Count -= 1u;
			return true;
		}
	}
	return false;
}
