#ifndef XRT_TAR_INTERNAL_H
#define XRT_TAR_INTERNAL_H

#ifndef XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_ROOT
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_TREE
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
#ifndef XRT_MODULE_SET
#define XRT_MODULE_SET
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif

#include "include/xrt_tar.h"

#define XRT_TAR_BLOCK_SIZE 512u

typedef struct {
	str sPath;
	str sLinkPath;
	str sUserName;
	str sGroupName;
	bool bHasSize;
	bool bHasMTime;
	bool bHasUID;
	bool bHasGID;
	uint64 iSize;
	int64 iMTime;
	uint64 iUID;
	uint64 iGID;
} xrttarpax;

struct xrttarreader {
	xreader* Reader;
	bool TakeReader;
	xrttaroptions Options;
	xrttarerror Error;
	xrttarentry Entry;
	str Name;
	str LinkName;
	str UserName;
	str GroupName;
	xrttarpax GlobalPax;
	uint64 EntryIndex;
	uint64 Offset;
	uint64 DataRemaining;
	uint64 DataPadding;
	uint64 TotalOutput;
	bool HasCurrent;
	bool EOFReached;
	bool Failed;
};

struct xrttarwriter {
	xwriter* Writer;
	xfile File; /* 借用 Writer 持有的事务文件，仅在流关闭前用于无分配回滚。 */
	bool TakeWriter;
	xbuffer* Memory;
	str Path;
	str Temporary;
	xrttaroptions Options;
	xrttarerror Error;
	uint64 EntryCount;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finalized;
	bool Closed;
	bool Failed;
};

void __xrtTarSetReaderError(xrttarreader* pReader, int iCode, const char* sFormat, ...);
void __xrtTarSetWriterError(xrttarwriter* pWriter, int iCode, const char* sFormat, ...);
void __xrtTarFailReader(xrttarreader* pReader, int iCode, const char* sFormat, ...);
void __xrtTarFailWriter(xrttarwriter* pWriter, int iCode, const char* sFormat, ...);
bool __xrtTarCopyOptions(xrttaroptions* pOutput, const xrttaroptions* pInput);
int __xrtTarIoFailureCode(void);
bool __xrtTarReadExact(xrttarreader* pReader, void* pData, uint64 iSize);
bool __xrtTarSkipBytes(xrttarreader* pReader, uint64 iSize);
bool __xrtTarWriteAll(xrttarwriter* pWriter, const void* pData, uint64 iSize);
bool __xrtTarExtractToWriter(xrttarreader* pReader, xwriter* pWriter, cstr sName);
bool __xrtTarEntryPathSafe(const char* sName, bool bDirectory, xrttarerror* pError,
	uint64 iEntryIndex);
void __xrtTarPaxClear(xrttarpax* pPax);

#endif
