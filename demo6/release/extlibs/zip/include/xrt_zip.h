#ifndef XRT_ZIP_H
#define XRT_ZIP_H

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

XXAPI void xrtZipOptionsInit(xrtzipoptions* pOptions);
XXAPI void xrtZipErrorInit(xrtziperror* pError);
XXAPI void xrtZipEntryInit(xrtzipentry* pEntry);

XXAPI xrtziparchive* xrtZipArchiveCreate(const xrtzipoptions* pOptions);
XXAPI xrtziparchive* xrtZipArchiveOpenMemory(const void* pData, size_t iSize, bool bWritable,
	const xrtzipoptions* pOptions);
XXAPI xrtziparchive* xrtZipArchiveOpenFile(str sPath, int iMode, const xrtzipoptions* pOptions);
XXAPI const xrtziperror* xrtZipArchiveError(const xrtziparchive* pArchive);
XXAPI bool xrtZipArchiveIsOpen(const xrtziparchive* pArchive);
XXAPI bool xrtZipArchiveIsMemory(const xrtziparchive* pArchive);
XXAPI uint64 xrtZipArchiveCount(xrtziparchive* pArchive);
XXAPI int64 xrtZipArchiveLocate(xrtziparchive* pArchive, str sName);
XXAPI bool xrtZipArchiveEntryAt(xrtziparchive* pArchive, uint64 iIndex, xrtzipentry* pEntry);
XXAPI xbuffer xrtZipArchiveReadAt(xrtziparchive* pArchive, uint64 iIndex);
XXAPI xbuffer xrtZipArchiveRead(xrtziparchive* pArchive, str sName);
XXAPI bool xrtZipArchiveAdd(xrtziparchive* pArchive, str sName, const void* pData, size_t iSize,
	bool bOverwrite, int iMethod, int iLevel);
XXAPI bool xrtZipArchiveAddFile(xrtziparchive* pArchive, str sName, str sSourcePath,
	bool bOverwrite, int iMethod, int iLevel);
XXAPI bool xrtZipArchiveAddDirectory(xrtziparchive* pArchive, str sName);
XXAPI bool xrtZipArchiveDelete(xrtziparchive* pArchive, str sName);
XXAPI bool xrtZipArchiveRename(xrtziparchive* pArchive, str sOldName, str sNewName);
XXAPI bool xrtZipArchiveSetComment(xrtziparchive* pArchive, str sComment);
XXAPI bool xrtZipArchiveSetEntryComment(xrtziparchive* pArchive, str sName, str sComment);
XXAPI bool xrtZipArchiveExtractAll(xrtziparchive* pArchive, str sDestination, bool bOverwrite);
XXAPI bool xrtZipArchiveClose(xrtziparchive* pArchive);
XXAPI xbuffer xrtZipArchiveTakeBuffer(xrtziparchive* pArchive);
XXAPI void xrtZipArchiveDestroy(xrtziparchive* pArchive);

#ifdef __cplusplus
}
#endif

#endif
