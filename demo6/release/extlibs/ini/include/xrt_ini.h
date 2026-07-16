#ifndef XRT_INI_H
#define XRT_INI_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_INI_OPTIONS_VERSION 1u

typedef enum {
	XRT_INI_OK = 0,
	XRT_INI_ERROR_ARGUMENT = 1,
	XRT_INI_ERROR_MEMORY = 2,
	XRT_INI_ERROR_FORMAT = 3,
	XRT_INI_ERROR_IO = 4,
	XRT_INI_ERROR_LIMIT = 5,
	XRT_INI_ERROR_DUPLICATE = 6,
	XRT_INI_ERROR_NOT_FOUND = 7,
	XRT_INI_ERROR_CONVERSION = 8,
	XRT_INI_ERROR_INTERPOLATION = 9
} xrtinistatus;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	bool bCaseSensitive;
	bool bStrictDuplicates;
	bool bInterpolation;
	bool bReserved;
	xrtresourcelimits tLimits;
} xrtinioptions;

typedef struct {
	int iCode;
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtinierror;

typedef struct xrtiniconfig xrtiniconfig;

XXAPI void xrtIniOptionsInit(xrtinioptions* pOptions);
XXAPI void xrtIniErrorInit(xrtinierror* pError);
XXAPI xrtiniconfig* xrtIniCreate(const xrtinioptions* pOptions);
XXAPI xrtiniconfig* xrtIniParse(xreader pReader, bool bTakeOwnership,
	const xrtinioptions* pOptions);
XXAPI xrtiniconfig* xrtIniParseMemory(const void* pData, uint64 iSize,
	const xrtinioptions* pOptions);
XXAPI xrtiniconfig* xrtIniLoad(str sPath, const xrtinioptions* pOptions);
XXAPI const xrtinierror* xrtIniError(const xrtiniconfig* pConfig);
XXAPI void xrtIniDestroy(xrtiniconfig* pConfig);

XXAPI uint32 xrtIniSectionCount(const xrtiniconfig* pConfig);
XXAPI const char* xrtIniSectionName(const xrtiniconfig* pConfig, uint32 iIndex);
XXAPI bool xrtIniHasSection(const xrtiniconfig* pConfig, str sSection);
XXAPI uint32 xrtIniKeyCount(const xrtiniconfig* pConfig, str sSection);
XXAPI const char* xrtIniKeyName(const xrtiniconfig* pConfig, str sSection, uint32 iIndex);
XXAPI bool xrtIniHas(const xrtiniconfig* pConfig, str sSection, str sKey);
XXAPI const char* xrtIniGetRaw(const xrtiniconfig* pConfig, str sSection, str sKey);
XXAPI str xrtIniGet(const xrtiniconfig* pConfig, str sSection, str sKey);
XXAPI bool xrtIniGetInt(const xrtiniconfig* pConfig, str sSection, str sKey, int64* pValue);
XXAPI bool xrtIniGetFloat(const xrtiniconfig* pConfig, str sSection, str sKey, double* pValue);
XXAPI bool xrtIniGetBool(const xrtiniconfig* pConfig, str sSection, str sKey, bool* pValue);

XXAPI bool xrtIniSet(xrtiniconfig* pConfig, str sSection, str sKey, str sValue);
XXAPI bool xrtIniRemove(xrtiniconfig* pConfig, str sSection, str sKey);
XXAPI bool xrtIniRemoveSection(xrtiniconfig* pConfig, str sSection);
XXAPI xbuffer xrtIniStringify(const xrtiniconfig* pConfig);
XXAPI bool xrtIniWrite(const xrtiniconfig* pConfig, xwriter pWriter, bool bTakeOwnership);
XXAPI bool xrtIniSave(const xrtiniconfig* pConfig, str sPath);

#ifdef __cplusplus
}
#endif

#endif
