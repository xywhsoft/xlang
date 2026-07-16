#ifndef XRT_TAR_H
#define XRT_TAR_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_TAR_OPTIONS_VERSION 1u
#define XRT_TAR_ENTRY_VERSION 1u

typedef enum {
	XRT_TAR_OK = 0,
	XRT_TAR_ERROR_ARGUMENT = 1,
	XRT_TAR_ERROR_MEMORY = 2,
	XRT_TAR_ERROR_FORMAT = 3,
	XRT_TAR_ERROR_IO = 4,
	XRT_TAR_ERROR_LIMIT = 5,
	XRT_TAR_ERROR_UNSAFE_PATH = 6,
	XRT_TAR_ERROR_EXISTS = 7,
	XRT_TAR_ERROR_STATE = 8,
	XRT_TAR_ERROR_UNSUPPORTED = 9
} xrttarstatus;

typedef enum {
	XRT_TAR_ENTRY_FILE = 0,
	XRT_TAR_ENTRY_DIRECTORY = 1,
	XRT_TAR_ENTRY_SYMLINK = 2,
	XRT_TAR_ENTRY_HARDLINK = 3,
	XRT_TAR_ENTRY_FIFO = 4,
	XRT_TAR_ENTRY_CHARACTER = 5,
	XRT_TAR_ENTRY_BLOCK = 6,
	XRT_TAR_ENTRY_OTHER = 7
} xrttarentrytype;

typedef struct {
	int iCode;
	uint64 iEntryIndex;
	uint64 iOffset;
	char sMessage[256];
} xrttarerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
	bool bAllowSymlinks;
	bool bAllowHardlinks;
	bool bAllowSpecialFiles;
	bool bPreservePermissions;
	uint32 iFileModeMask;
	uint32 iReserved;
} xrttaroptions;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint64 iIndex;
	uint64 iSizeBytes;
	int64 iMTime;
	uint64 iUID;
	uint64 iGID;
	uint32 iMode;
	uint32 iDeviceMajor;
	uint32 iDeviceMinor;
	int iType;
	const char* sName;
	const char* sLinkName;
	const char* sUserName;
	const char* sGroupName;
} xrttarentry;

typedef struct xrttarreader xrttarreader;
typedef struct xrttarwriter xrttarwriter;

XXAPI void xrtTarOptionsInit(xrttaroptions* pOptions);
XXAPI void xrtTarErrorInit(xrttarerror* pError);
XXAPI void xrtTarEntryInit(xrttarentry* pEntry);

XXAPI xrttarreader* xrtTarReaderCreate(xreader pReader, bool bTakeOwnership,
	const xrttaroptions* pOptions);
XXAPI xrttarreader* xrtTarReaderOpenMemory(const void* pData, uint64 iSize,
	const xrttaroptions* pOptions);
XXAPI xrttarreader* xrtTarReaderOpenFile(str sPath, const xrttaroptions* pOptions);
XXAPI const xrttarerror* xrtTarReaderError(const xrttarreader* pReader);
XXAPI bool xrtTarReaderNext(xrttarreader* pReader, xrttarentry* pEntry);
XXAPI bool xrtTarReaderEOF(const xrttarreader* pReader);
XXAPI xbuffer xrtTarReaderRead(xrttarreader* pReader);
XXAPI bool xrtTarReaderCopy(xrttarreader* pReader, xwriter pWriter);
XXAPI bool xrtTarReaderSkip(xrttarreader* pReader);
XXAPI bool xrtTarReaderExtractAll(xrttarreader* pReader, str sDestination, bool bOverwrite);
XXAPI void xrtTarReaderDestroy(xrttarreader* pReader);

XXAPI xrttarwriter* xrtTarWriterCreate(xwriter pWriter, bool bTakeOwnership,
	const xrttaroptions* pOptions);
XXAPI xrttarwriter* xrtTarWriterCreateMemory(const xrttaroptions* pOptions);
XXAPI xrttarwriter* xrtTarWriterOpenFile(str sPath, const xrttaroptions* pOptions);
XXAPI const xrttarerror* xrtTarWriterError(const xrttarwriter* pWriter);
XXAPI bool xrtTarWriterAdd(xrttarwriter* pWriter, const xrttarentry* pEntry,
	const void* pData, uint64 iSize);
XXAPI bool xrtTarWriterAddFile(xrttarwriter* pWriter, str sName, str sSourcePath,
	uint32 iMode, int64 iMTime);
XXAPI bool xrtTarWriterAddDirectory(xrttarwriter* pWriter, str sName,
	uint32 iMode, int64 iMTime);
XXAPI bool xrtTarWriterAddSymlink(xrttarwriter* pWriter, str sName, str sTarget,
	uint32 iMode, int64 iMTime);
XXAPI bool xrtTarWriterClose(xrttarwriter* pWriter);
XXAPI xbuffer xrtTarWriterTakeBuffer(xrttarwriter* pWriter);
XXAPI void xrtTarWriterDestroy(xrttarwriter* pWriter);

#ifdef __cplusplus
}
#endif

#endif
