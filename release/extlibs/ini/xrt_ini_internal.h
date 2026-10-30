#ifndef XRT_INI_INTERNAL_H
#define XRT_INI_INTERNAL_H

#ifndef XRT_MODULE_CHARSET
#define XRT_MODULE_CHARSET
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#include "xrt.h"
#include "include/xrt_ini.h"

typedef struct {
	str Key;
	str Value;
	str Leading;
	uint64 Line;
} xrt_ini_entry;

typedef struct {
	str Name;
	str Leading;
	xrt_ini_entry* Entries;
	uint32 Count;
	uint32 Capacity;
} xrt_ini_section;

struct xrtiniconfig {
	xrtinioptions Options;
	xrtinierror Error;
	xrt_ini_section* Sections;
	uint32 Count;
	uint32 Capacity;
	str Trailing;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 Nodes;
	bool Failed;
};

bool __xrtIniCopyOptions(xrtinioptions* pOutput, const xrtinioptions* pInput);
void __xrtIniSetError(xrtiniconfig* pConfig, int iCode, uint64 iLine,
	uint64 iColumn, uint64 iOffset, const char* sFormat, ...);
bool __xrtIniNameEqual(const xrtiniconfig* pConfig, const char* sLeft, const char* sRight);
xrt_ini_section* __xrtIniFindSection(const xrtiniconfig* pConfig, const char* sName);
xrt_ini_entry* __xrtIniFindEntry(const xrtiniconfig* pConfig,
	const xrt_ini_section* pSection, const char* sKey);
xrt_ini_section* __xrtIniAddSection(xrtiniconfig* pConfig, const char* sName,
	const char* sLeading, uint64 iLine, bool bFromParser);
xrt_ini_entry* __xrtIniAddEntry(xrtiniconfig* pConfig, xrt_ini_section* pSection,
	const char* sKey, const char* sValue, const char* sLeading,
	uint64 iLine, bool bFromParser);

#endif
