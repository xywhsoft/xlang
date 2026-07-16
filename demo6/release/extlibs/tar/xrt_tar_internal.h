#ifndef XRT_TAR_INTERNAL_H
#define XRT_TAR_INTERNAL_H

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
	xreader Reader;
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
	xwriter Writer;
	bool TakeWriter;
	xbuffer Memory;
	xrttaroptions Options;
	xrttarerror Error;
	uint64 EntryCount;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Closed;
	bool Failed;
};

void __xrtTarSetReaderError(xrttarreader* pReader, int iCode, const char* sFormat, ...);
void __xrtTarSetWriterError(xrttarwriter* pWriter, int iCode, const char* sFormat, ...);
bool __xrtTarCopyOptions(xrttaroptions* pOutput, const xrttaroptions* pInput);
bool __xrtTarReadExact(xrttarreader* pReader, void* pData, uint64 iSize);
bool __xrtTarSkipBytes(xrttarreader* pReader, uint64 iSize);
bool __xrtTarWriteAll(xrttarwriter* pWriter, const void* pData, uint64 iSize);
bool __xrtTarEntryPathSafe(const char* sName, bool bDirectory, xrttarerror* pError,
	uint64 iEntryIndex);
void __xrtTarPaxClear(xrttarpax* pPax);

#endif
