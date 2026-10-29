#ifndef XRT_ZIP_H
#define XRT_ZIP_H

/* ZIP 公共契约返回 Buffer，并允许把条目流式写入通用 Writer。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_ZIP_OPTIONS_VERSION 1u
#define XRT_ZIP_ENTRY_VERSION 1u

typedef enum {
	XRT_ZIP_OK = 0,
	XRT_ZIP_ERROR_ARGUMENT = 1,
	XRT_ZIP_ERROR_MEMORY = 2,
	XRT_ZIP_ERROR_FORMAT = 3,
	XRT_ZIP_ERROR_NOT_FOUND = 4,
	XRT_ZIP_ERROR_EXISTS = 5,
	XRT_ZIP_ERROR_IO = 6,
	XRT_ZIP_ERROR_LIMIT = 7,
	XRT_ZIP_ERROR_UNSAFE_PATH = 8,
	XRT_ZIP_ERROR_STATE = 9,
	XRT_ZIP_ERROR_BACKEND = 10
} xrtzipstatus;

typedef enum {
	XRT_ZIP_MODE_READ = 0,
	XRT_ZIP_MODE_WRITE = 1,
	XRT_ZIP_MODE_APPEND = 2,
	XRT_ZIP_MODE_EXCLUSIVE = 3
} xrtzipmode;

typedef enum {
	XRT_ZIP_METHOD_DEFAULT = -1,
	XRT_ZIP_METHOD_STORE = 0,
	XRT_ZIP_METHOD_DEFLATE = 8
} xrtzipmethod;

typedef struct {
	int iCode;
	int iBackendCode;
	int iSystemCode;
	uint64 iEntryIndex;
	char sMessage[256];
} xrtziperror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iDefaultMethod;
	int iDefaultLevel;
	uint32 iReserved;
	xrtresourcelimits tLimits;
} xrtzipoptions;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint64 iIndex;
	uint64 iUncompressedSize;
	uint64 iCompressedSize;
	int64 iMTime;
	uint32 iCRC32;
	int iCompressionMethod;
	int iEncryptionMethod;
	uint32 iUnixMode;
	bool bDirectory;
	const char* sName;
	const char* sComment;
} xrtzipentry;

typedef struct xrtziparchive xrtziparchive;

XRT_API void xrtZipOptionsInit(xrtzipoptions* pOptions);
XRT_API void xrtZipErrorInit(xrtziperror* pError);
XRT_API void xrtZipEntryInit(xrtzipentry* pEntry);

XRT_API xrtziparchive* xrtZipArchiveCreate(const xrtzipoptions* pOptions);
XRT_API xrtziparchive* xrtZipArchiveOpenMemory(const void* pData, size_t iSize, bool bWritable,
	const xrtzipoptions* pOptions);
XRT_API xrtziparchive* xrtZipArchiveOpenFile(cstr sPath, int iMode, const xrtzipoptions* pOptions);
XRT_API const xrtziperror* xrtZipArchiveError(const xrtziparchive* pArchive);
XRT_API bool xrtZipArchiveIsOpen(const xrtziparchive* pArchive);
XRT_API bool xrtZipArchiveIsMemory(const xrtziparchive* pArchive);
XRT_API uint64 xrtZipArchiveCount(xrtziparchive* pArchive);
XRT_API int64 xrtZipArchiveLocate(xrtziparchive* pArchive, cstr sName);
XRT_API bool xrtZipArchiveEntryAt(xrtziparchive* pArchive, uint64 iIndex, xrtzipentry* pEntry);
XRT_API bool xrtZipArchiveReadAtTo(xrtziparchive* pArchive, uint64 iIndex, xwriter* pWriter);
XRT_API bool xrtZipArchiveReadTo(xrtziparchive* pArchive, cstr sName, xwriter* pWriter);
XRT_API xbuffer* xrtZipArchiveReadAt(xrtziparchive* pArchive, uint64 iIndex);
XRT_API xbuffer* xrtZipArchiveRead(xrtziparchive* pArchive, cstr sName);
XRT_API bool xrtZipArchiveAdd(xrtziparchive* pArchive, cstr sName, const void* pData, size_t iSize,
	bool bOverwrite, int iMethod, int iLevel);
XRT_API bool xrtZipArchiveAddFile(xrtziparchive* pArchive, cstr sName, cstr sSourcePath,
	bool bOverwrite, int iMethod, int iLevel);
XRT_API bool xrtZipArchiveAddDirectory(xrtziparchive* pArchive, cstr sName);
XRT_API bool xrtZipArchiveDelete(xrtziparchive* pArchive, cstr sName);
XRT_API bool xrtZipArchiveRename(xrtziparchive* pArchive, cstr sOldName, cstr sNewName);
XRT_API bool xrtZipArchiveSetComment(xrtziparchive* pArchive, cstr sComment);
XRT_API bool xrtZipArchiveSetEntryComment(xrtziparchive* pArchive, cstr sName, cstr sComment);
XRT_API bool xrtZipArchiveExtractAll(xrtziparchive* pArchive, cstr sDestination, bool bOverwrite);
XRT_API bool xrtZipArchiveClose(xrtziparchive* pArchive);
XRT_API xbuffer* xrtZipArchiveTakeBuffer(xrtziparchive* pArchive);
XRT_API void xrtZipArchiveDestroy(xrtziparchive* pArchive);

#ifdef __cplusplus
}
#endif

#endif
