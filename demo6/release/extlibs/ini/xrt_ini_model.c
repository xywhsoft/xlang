#include "xrt_ini_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



XXAPI void xrtIniOptionsInit(xrtinioptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_INI_OPTIONS_VERSION;
	pOptions->bCaseSensitive = TRUE;
	pOptions->bStrictDuplicates = TRUE;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI void xrtIniErrorInit(xrtinierror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

bool __xrtIniCopyOptions(xrtinioptions* pOutput, const xrtinioptions* pInput)
{
	xrtIniOptionsInit(pOutput);
	if ( pInput == NULL ) { return TRUE; }
	if ( pInput->iVersion != XRT_INI_OPTIONS_VERSION ||
		pInput->iSize < offsetof(xrtinioptions, tLimits) + sizeof(xrtresourcelimits) ) {
		return FALSE;
	}
	memcpy(pOutput, pInput, pInput->iSize < sizeof(*pOutput) ?
		pInput->iSize : sizeof(*pOutput));
	pOutput->iSize = sizeof(*pOutput);
	pOutput->iVersion = XRT_INI_OPTIONS_VERSION;
	return TRUE;
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
	pConfig->Failed = TRUE;
	xrtSetError(xrtCopyStr((str)pConfig->Error.sMessage, 0u), TRUE);
}

static void __xrtIniClearError(xrtiniconfig* pConfig)
{
	if ( pConfig == NULL ) { return; }
	xrtIniErrorInit(&pConfig->Error);
	pConfig->Failed = FALSE;
}

bool __xrtIniNameEqual(const xrtiniconfig* pConfig, const char* sLeft, const char* sRight)
{
	if ( sLeft == NULL || sRight == NULL ) { return sLeft == sRight; }
	return pConfig != NULL && !pConfig->Options.bCaseSensitive ?
		xrtStrComp((str)sLeft, (str)sRight, 0u, TRUE) == 0 : strcmp(sLeft, sRight) == 0;
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
		return FALSE;
	}
	return TRUE;
}

xrt_ini_section* __xrtIniAddSection(xrtiniconfig* pConfig, const char* sName,
	const char* sLeading, uint64 iLine, bool bFromParser)
{
	xrt_ini_section* pExisting;
	xrt_ini_section* pSections;
	xrt_ini_section* pSection;
	uint32 iCapacity;
	if ( pConfig == NULL || sName == NULL || !xrtIsUTF8((str)sName, strlen(sName)) ) {
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
	pSection->Name = xrtCopyStr((str)sName, 0u);
	pSection->Leading = xrtCopyStr((str)(sLeading != NULL ? sLeading : ""), 0u);
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

bool __xrtIniAddEntry(xrtiniconfig* pConfig, xrt_ini_section* pSection,
	const char* sKey, const char* sValue, const char* sLeading,
	uint64 iLine, bool bFromParser)
{
	xrt_ini_entry* pEntry;
	xrt_ini_entry* pEntries;
	str sCopy;
	uint32 iCapacity;
	if ( pConfig == NULL || pSection == NULL || sKey == NULL || sValue == NULL ||
		sKey[0] == 0 || !xrtIsUTF8((str)sKey, strlen(sKey)) ||
		!xrtIsUTF8((str)sValue, strlen(sValue)) ) {
		if ( pConfig != NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_ARGUMENT, iLine, 1u, 0u,
				"invalid INI key or value");
		}
		return FALSE;
	}
	pEntry = __xrtIniFindEntry(pConfig, pSection, sKey);
	if ( pEntry != NULL ) {
		if ( bFromParser && pConfig->Options.bStrictDuplicates ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_DUPLICATE, iLine, 1u, 0u,
				"duplicate INI key: %s", sKey);
			return FALSE;
		}
		sCopy = xrtCopyStr((str)sValue, 0u);
		if ( sCopy == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
				"cannot copy INI value");
			return FALSE;
		}
		xrtFree(pEntry->Value);
		pEntry->Value = sCopy;
		return TRUE;
	}
	if ( !__xrtIniCanAddNode(pConfig) ) { return FALSE; }
	if ( pSection->Count == pSection->Capacity ) {
		iCapacity = pSection->Capacity != 0u ? pSection->Capacity * 2u : 8u;
		if ( iCapacity < pSection->Capacity ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, 0u,
				"too many INI keys");
			return FALSE;
		}
		pEntries = (xrt_ini_entry*)xrtRealloc(pSection->Entries,
			(size_t)iCapacity * sizeof(*pEntries));
		if ( pEntries == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
				"cannot grow INI key table");
			return FALSE;
		}
		pSection->Entries = pEntries;
		pSection->Capacity = iCapacity;
	}
	pEntry = &pSection->Entries[pSection->Count];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->Key = xrtCopyStr((str)sKey, 0u);
	pEntry->Value = xrtCopyStr((str)sValue, 0u);
	pEntry->Leading = xrtCopyStr((str)(sLeading != NULL ? sLeading : ""), 0u);
	pEntry->Line = iLine;
	if ( pEntry->Key == NULL || pEntry->Value == NULL || pEntry->Leading == NULL ) {
		xrtFree(pEntry->Key);
		xrtFree(pEntry->Value);
		xrtFree(pEntry->Leading);
		memset(pEntry, 0, sizeof(*pEntry));
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, 0u,
			"cannot copy INI entry");
		return FALSE;
	}
	pSection->Count += 1u;
	pConfig->Nodes += 1u;
	return TRUE;
}



XXAPI xrtiniconfig* xrtIniCreate(const xrtinioptions* pOptions)
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

XXAPI const xrtinierror* xrtIniError(const xrtiniconfig* pConfig)
{
	return pConfig != NULL ? &pConfig->Error : NULL;
}

XXAPI void xrtIniDestroy(xrtiniconfig* pConfig)
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

XXAPI uint32 xrtIniSectionCount(const xrtiniconfig* pConfig)
{
	return pConfig != NULL ? pConfig->Count : 0u;
}

XXAPI const char* xrtIniSectionName(const xrtiniconfig* pConfig, uint32 iIndex)
{
	return pConfig != NULL && iIndex < pConfig->Count ?
		(const char*)pConfig->Sections[iIndex].Name : NULL;
}

XXAPI bool xrtIniHasSection(const xrtiniconfig* pConfig, str sSection)
{
	return __xrtIniFindSection(pConfig, (const char*)(sSection != NULL ? sSection : (str)"")) != NULL;
}

XXAPI uint32 xrtIniKeyCount(const xrtiniconfig* pConfig, str sSection)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return pSection != NULL ? pSection->Count : 0u;
}

XXAPI const char* xrtIniKeyName(const xrtiniconfig* pConfig, str sSection, uint32 iIndex)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return pSection != NULL && iIndex < pSection->Count ?
		(const char*)pSection->Entries[iIndex].Key : NULL;
}

XXAPI bool xrtIniHas(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	return __xrtIniFindEntry(pConfig, pSection, (const char*)sKey) != NULL;
}

XXAPI const char* xrtIniGetRaw(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	xrt_ini_entry* pEntry = __xrtIniFindEntry(pConfig, pSection, (const char*)sKey);
	return pEntry != NULL ? (const char*)pEntry->Value : NULL;
}



static bool __xrtIniAppendResolved(xrtiniconfig* pConfig, xbuffer pOutput,
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
		return FALSE;
	}
	for ( i = 0u; i < iDepth; ++i ) {
		if ( __xrtIniNameEqual(pConfig, pSections[i], sSection) &&
			__xrtIniNameEqual(pConfig, pKeys[i], sKey) ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_INTERPOLATION, 0u, 0u, 0u,
				"INI interpolation cycle at %s.%s", sSection, sKey);
			return FALSE;
		}
	}
	sValue = xrtIniGetRaw(pConfig, (str)sSection, (str)sKey);
	if ( sValue == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_NOT_FOUND, 0u, 0u, 0u,
			"INI key does not exist: %s.%s", sSection, sKey);
		return FALSE;
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
				return FALSE;
			}
			if ( p != pStart && !xrtBufferAppend(pOutput, (ptr)pStart,
				(uint32)(p - pStart), XBUF_BINARY) ) { return FALSE; }
			pDot = memchr(p + 2, '.', (size_t)(pClose - (p + 2)));
			if ( pDot != NULL ) {
				sRefSection = xrtCopyStr((str)(p + 2), (size_t)(pDot - (p + 2)));
				sRefKey = xrtCopyStr((str)(pDot + 1), (size_t)(pClose - (pDot + 1)));
			} else {
				sRefSection = xrtCopyStr((str)sSection, 0u);
				sRefKey = xrtCopyStr((str)(p + 2), (size_t)(pClose - (p + 2)));
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
				return FALSE;
			}
			xrtFree(sRefSection);
			xrtFree(sRefKey);
			p = pClose + 1;
			pStart = p;
			continue;
		}
		++p;
	}
	if ( p != pStart && !xrtBufferAppend(pOutput, (ptr)pStart,
		(uint32)(p - pStart), XBUF_BINARY) ) { return FALSE; }
	return TRUE;
}

XXAPI str xrtIniGet(const xrtiniconfig* pConst, str sSection, str sKey)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	const char* sRaw;
	xbuffer pOutput;
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
	if ( !pConfig->Options.bInterpolation ) { return xrtCopyStr((str)sRaw, 0u); }
	iMaxDepth = pConfig->Options.tLimits.iMaxDepth != 0u ?
		pConfig->Options.tLimits.iMaxDepth : 64u;
	pStack = (const char**)xrtMalloc((size_t)iMaxDepth * sizeof(char*) * 2u);
	pOutput = xrtBufferCreate(256u);
	if ( pStack == NULL || pOutput == NULL ) {
		xrtFree(pStack);
		xrtBufferDestroy(pOutput);
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI interpolation state");
		return NULL;
	}
	pSections = pStack;
	pKeys = pStack + iMaxDepth;
	if ( !__xrtIniAppendResolved(pConfig, pOutput, (const char*)sSection,
		(const char*)sKey, pSections, pKeys, 0u, iMaxDepth) ||
		(pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
		 pOutput->Length > pConfig->Options.tLimits.iMaxOutputBytes) ) {
		if ( !pConfig->Failed ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
				"resolved INI value exceeds the configured limit");
		}
		xrtFree(pStack);
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	sResult = pOutput->Length != 0u ?
		xrtCopyStr((str)pOutput->Buffer, pOutput->Length) : xrtCopyStr((str)"", 0u);
	xrtFree(pStack);
	xrtBufferDestroy(pOutput);
	return sResult;
}



static str __xrtIniValueForConversion(const xrtiniconfig* pConfig, str sSection, str sKey)
{
	return xrtIniGet(pConfig, sSection, sKey);
}

XXAPI bool xrtIniGetInt(const xrtiniconfig* pConst, str sSection, str sKey, int64* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	char* pEnd;
	long long iValue;
	if ( pConfig == NULL || pValue == NULL ) { return FALSE; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return FALSE; }
	errno = 0;
	iValue = strtoll((const char*)sValue, &pEnd, 10);
	if ( errno == ERANGE || pEnd == (char*)sValue || *pEnd != 0 ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid int: %s", sValue);
		xrtFree(sValue);
		return FALSE;
	}
	*pValue = (int64)iValue;
	xrtFree(sValue);
	return TRUE;
}

XXAPI bool xrtIniGetFloat(const xrtiniconfig* pConst, str sSection, str sKey, double* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	char* pEnd;
	double fValue;
	if ( pConfig == NULL || pValue == NULL ) { return FALSE; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return FALSE; }
	errno = 0;
	fValue = strtod((const char*)sValue, &pEnd);
	if ( errno == ERANGE || pEnd == (char*)sValue || *pEnd != 0 ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid float: %s", sValue);
		xrtFree(sValue);
		return FALSE;
	}
	*pValue = fValue;
	xrtFree(sValue);
	return TRUE;
}

XXAPI bool xrtIniGetBool(const xrtiniconfig* pConst, str sSection, str sKey, bool* pValue)
{
	xrtiniconfig* pConfig = (xrtiniconfig*)pConst;
	str sValue;
	if ( pConfig == NULL || pValue == NULL ) { return FALSE; }
	sValue = __xrtIniValueForConversion(pConfig, sSection, sKey);
	if ( sValue == NULL ) { return FALSE; }
	if ( xrtStrComp(sValue, (str)"true", 0u, TRUE) == 0 ||
		xrtStrComp(sValue, (str)"yes", 0u, TRUE) == 0 ||
		xrtStrComp(sValue, (str)"on", 0u, TRUE) == 0 || strcmp((char*)sValue, "1") == 0 ) {
		*pValue = TRUE;
	} else if ( xrtStrComp(sValue, (str)"false", 0u, TRUE) == 0 ||
		xrtStrComp(sValue, (str)"no", 0u, TRUE) == 0 ||
		xrtStrComp(sValue, (str)"off", 0u, TRUE) == 0 || strcmp((char*)sValue, "0") == 0 ) {
		*pValue = FALSE;
	} else {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_CONVERSION, 0u, 0u, 0u,
			"INI value is not a valid bool: %s", sValue);
		xrtFree(sValue);
		return FALSE;
	}
	xrtFree(sValue);
	return TRUE;
}



XXAPI bool xrtIniSet(xrtiniconfig* pConfig, str sSection, str sKey, str sValue)
{
	xrt_ini_section* pSection;
	if ( pConfig == NULL || sKey == NULL || sValue == NULL ) { return FALSE; }
	__xrtIniClearError(pConfig);
	sSection = sSection != NULL ? sSection : (str)"";
	pSection = __xrtIniFindSection(pConfig, (const char*)sSection);
	if ( pSection == NULL ) {
		pSection = __xrtIniAddSection(pConfig, (const char*)sSection, "", 0u, FALSE);
	}
	return pSection != NULL && __xrtIniAddEntry(pConfig, pSection,
		(const char*)sKey, (const char*)sValue, "", 0u, FALSE);
}

XXAPI bool xrtIniRemove(xrtiniconfig* pConfig, str sSection, str sKey)
{
	xrt_ini_section* pSection;
	uint32 i;
	if ( pConfig == NULL || sKey == NULL ) { return FALSE; }
	__xrtIniClearError(pConfig);
	pSection = __xrtIniFindSection(pConfig,
		(const char*)(sSection != NULL ? sSection : (str)""));
	if ( pSection == NULL ) { return FALSE; }
	for ( i = 0u; i < pSection->Count; ++i ) {
		if ( __xrtIniNameEqual(pConfig, (const char*)pSection->Entries[i].Key, (const char*)sKey) ) {
			xrtFree(pSection->Entries[i].Key);
			xrtFree(pSection->Entries[i].Value);
			xrtFree(pSection->Entries[i].Leading);
			memmove(&pSection->Entries[i], &pSection->Entries[i + 1u],
				(size_t)(pSection->Count - i - 1u) * sizeof(*pSection->Entries));
			pSection->Count -= 1u;
			pConfig->Nodes -= 1u;
			return TRUE;
		}
	}
	return FALSE;
}

XXAPI bool xrtIniRemoveSection(xrtiniconfig* pConfig, str sSection)
{
	uint32 i;
	uint32 j;
	if ( pConfig == NULL || sSection == NULL ) { return FALSE; }
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
			return TRUE;
		}
	}
	return FALSE;
}
