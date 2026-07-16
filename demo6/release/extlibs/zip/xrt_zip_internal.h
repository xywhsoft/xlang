#ifndef XRT_ZIP_INTERNAL_H
#define XRT_ZIP_INTERNAL_H

#include "include/xrt_zip.h"
#include "../_libzip/third_party/libzip/lib/zip.h"

typedef struct {
	xbuffer Base;
	xbuffer Write;
	xwriter Writer;
	uint64 ReadPosition;
	uint64 MaxBytes;
	zip_error_t Error;
	bool SourceFreed;
} xrtzipmemorysource;

struct xrtziparchive {
	zip_t* Zip;
	xrtzipmemorysource* Memory;
	str Path;
	xrtzipoptions Options;
	xrtziperror Error;
	bool ReadOnly;
	bool Committed;
	bool Saved;
};

xrtzipmemorysource* __xrtZipMemorySourceCreate(const void* pData, size_t iSize, uint64 iMaxBytes,
	zip_error_t* pError, zip_source_t** pSource);
void __xrtZipMemorySourceDestroy(xrtzipmemorysource* pMemory);
xbuffer __xrtZipMemorySourceTake(xrtzipmemorysource* pMemory);

void __xrtZipSetError(xrtziparchive* pArchive, int iCode, int iBackend, int iSystem,
	uint64 iEntry, const char* sFormat, ...);
void __xrtZipSetBackendError(xrtziparchive* pArchive, const zip_error_t* pError,
	uint64 iEntry, const char* sAction);
bool __xrtZipCheckOpen(xrtziparchive* pArchive, bool bWrite);
bool __xrtZipEntryPathSafe(const char* sName, bool bDirectory, xrtziparchive* pArchive,
	uint64 iEntry);

#endif
