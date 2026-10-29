#ifndef XRT_TAR_H
#define XRT_TAR_H

/* TAR 公开契约同时提供流式 IO 和 Buffer 便利路径。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif

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
	XRT_TAR_ERROR_UNSUPPORTED = 9,
	XRT_TAR_ERROR_CANCELLED = 10,
	XRT_TAR_ERROR_CALLBACK = 11
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

typedef enum {
	XRT_TAR_SELECT_ERROR = -1,
	XRT_TAR_SELECT_SKIP = 0,
	XRT_TAR_SELECT_EXTRACT = 1
} xrttarselect;

typedef struct {
	int iCode;
	uint64 iEntryIndex;
	uint64 iOffset;
	char sMessage[256];
} xrttarerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	/* TAR 不使用深度、压缩倍率和资源标志；这些字段必须为零。 */
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

/* 回调参数只在调用期间借用；普通条目返回 false 可取消。FINAL 表示已完成，其返回值不影响结果。 */
typedef bool (*xrttarprogressproc)(const xrttarentry* pEntry,
	const xrtprogress* pProgress, ptr pUserData);

/* 选择器只在调用期间借用条目；返回 ERROR 时可先设置 XRT 结构化错误。 */
typedef xrttarselect (*xrttarselectproc)(const xrttarentry* pEntry,
	ptr pUserData);

XRT_API void xrtTarOptionsInit(xrttaroptions* pOptions);
XRT_API void xrtTarErrorInit(xrttarerror* pError);
XRT_API void xrtTarEntryInit(xrttarentry* pEntry);

XRT_API xrttarreader* xrtTarReaderCreate(xreader* pReader, bool bTakeOwnership,
	const xrttaroptions* pOptions);
XRT_API xrttarreader* xrtTarReaderOpenMemory(const void* pData, uint64 iSize,
	const xrttaroptions* pOptions);
XRT_API xrttarreader* xrtTarReaderOpenFile(cstr sPath, const xrttaroptions* pOptions);
XRT_API const xrttarerror* xrtTarReaderError(const xrttarreader* pReader);
XRT_API bool xrtTarReaderNext(xrttarreader* pReader, xrttarentry* pEntry);
XRT_API bool xrtTarReaderEOF(const xrttarreader* pReader);
XRT_API xbuffer* xrtTarReaderRead(xrttarreader* pReader);
XRT_API bool xrtTarReaderReadTo(xrttarreader* pReader, xwriter* pWriter);
XRT_API bool xrtTarReaderSkip(xrttarreader* pReader);
XRT_API bool xrtTarReaderExtract(xrttarreader* pReader, cstr sDestination,
	bool bOverwrite, xrttarprogressproc pProgress, ptr pUserData);
XRT_API bool xrtTarReaderExtractWhere(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite,
	xrttarselectproc pSelect, ptr pSelectData,
	xrttarprogressproc pProgress, ptr pProgressData);
XRT_API bool xrtTarReaderExtractNames(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite,
	const xstrview* pNames, size_t iNameCount);
XRT_API bool xrtTarReaderExtractAll(xrttarreader* pReader,
	cstr sDestination, bool bOverwrite);
XRT_API void xrtTarReaderDestroy(xrttarreader* pReader);

XRT_API xrttarwriter* xrtTarWriterCreate(xwriter* pWriter, bool bTakeOwnership,
	const xrttaroptions* pOptions);
XRT_API xrttarwriter* xrtTarWriterCreateMemory(const xrttaroptions* pOptions);
XRT_API xrttarwriter* xrtTarWriterOpenFile(cstr sPath, const xrttaroptions* pOptions);
XRT_API xrttarwriter* xrtTarWriterOpenAppend(cstr sPath, const xrttaroptions* pOptions);
XRT_API const xrttarerror* xrtTarWriterError(const xrttarwriter* pWriter);
/* 非文件条目不得携带正文，iSize 与 iSizeBytes 都必须为零。 */
XRT_API bool xrtTarWriterAdd(xrttarwriter* pWriter, const xrttarentry* pEntry,
	const void* pData, uint64 iSize);
XRT_API bool xrtTarWriterAddFrom(xrttarwriter* pWriter,
	const xrttarentry* pEntry, xreader* pReader, uint64 iSize);
XRT_API bool xrtTarWriterCopyCurrent(xrttarwriter* pWriter,
	xrttarreader* pReader);
XRT_API bool xrtTarWriterAddFile(xrttarwriter* pWriter, cstr sName, cstr sSourcePath,
	uint32 iMode, int64 iMTime);
XRT_API bool xrtTarWriterAddDirectory(xrttarwriter* pWriter, cstr sName,
	uint32 iMode, int64 iMTime);
XRT_API bool xrtTarWriterAddSymlink(xrttarwriter* pWriter, cstr sName, cstr sTarget,
	uint32 iMode, int64 iMTime);
XRT_API bool xrtTarWriterAddTree(xrttarwriter* pWriter, cstr sArchiveRoot,
	cstr sSourceDirectory, bool bRecursive);
XRT_API bool xrtTarWriterClose(xrttarwriter* pWriter);
XRT_API xbuffer* xrtTarWriterTakeBuffer(xrttarwriter* pWriter);
XRT_API void xrtTarWriterDestroy(xrttarwriter* pWriter);

#ifdef __cplusplus
}
#endif

#endif
