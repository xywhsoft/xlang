#ifndef XRT_ZIP_INTERNAL_H
#define XRT_ZIP_INTERNAL_H

#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
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
#ifndef XRT_MODULE_PATH_SAFE
#define XRT_MODULE_PATH_SAFE
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif

#include "include/xrt_zip.h"
#include "../_libzip/third_party/libzip/lib/zip.h"

typedef struct {
	xbuffer* Base;
	xbuffer* Write;
	xwriter* Writer;
	uint64 ReadPosition;
	uint64 MaxBytes;
	zip_error_t Error;
} xrtzipmemorysource;

struct xrtziparchive {
	zip_t* Zip;
	xrtzipmemorysource* Memory;
	str Path;
	str TempPath;
	xrtzipoptions Options;
	xrtziperror Error;
	bool ReadOnly;
	bool Committed;
	bool Saved;
	bool ReplaceTarget;
};

xrtzipmemorysource* __xrtZipMemorySourceCreate(const void* pData, size_t iSize, uint64 iMaxBytes,
	zip_error_t* pError, zip_source_t** pSource);
void __xrtZipMemorySourceDestroy(xrtzipmemorysource* pMemory);
xbuffer* __xrtZipMemorySourceTake(xrtzipmemorysource* pMemory);
xbuffer* __xrtZipBufferCreate(size_t iCapacity);

void __xrtZipSetError(xrtziparchive* pArchive, int iCode, int iBackend, int iSystem,
	uint64 iEntry, const char* sFormat, ...);
void __xrtZipSetBackendError(xrtziparchive* pArchive, const zip_error_t* pError,
	uint64 iEntry, const char* sAction);
void __xrtZipSetOutputError(xrtziparchive* pArchive, uint64 iEntry,
	int iBackend, const char* sMessage, cstr sPath);
bool __xrtZipCheckOpen(xrtziparchive* pArchive, bool bWrite);
bool __xrtZipEntryPathSafe(const char* sName, bool bDirectory, xrtziparchive* pArchive,
	uint64 iEntry);
bool __xrtZipExtractEntryToWriter(xrtziparchive* pArchive, uint64 iEntry,
	xwriter* pWriter, cstr sOutput);

#endif
