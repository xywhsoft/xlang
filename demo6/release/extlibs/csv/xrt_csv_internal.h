#ifndef XRT_CSV_INTERNAL_H
#define XRT_CSV_INTERNAL_H

#include "include/xrt_csv.h"

struct xrtcsvreader {
	xreader* Reader;
	bool TakeReader;
	xrtcsvdialect Dialect;
	xrtcsverror Error;
	xerror* Failure;
	xbuffer* Field;
	str* Fields;
	uint32 FieldCount;
	uint32 FieldCapacity;
	uint64 RowNumber;
	uint64 Line;
	uint64 Column;
	uint64 Offset;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 TotalFields;
	unsigned char Input[16384];
	size_t InputIndex;
	size_t InputSize;
	uint64 LastLine;
	uint64 LastColumn;
	uint64 LastOffset;
	unsigned char Prefix[3];
	uint32 PrefixIndex;
	uint32 PrefixCount;
	int Pending;
	bool PrefixReady;
	bool EOFReached;
	bool Failed;
	bool Closed;
	bool PreviousCR;
	bool Busy;
};

struct xrtcsvwriter {
	xwriter* Writer;
	bool TakeWriter;
	xbuffer* Memory;
	xrtcsvdialect Dialect;
	xrtcsverror Error;
	xerror* Failure;
	uint64 RowNumber;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 TotalFields;
	bool Closed;
	bool Failed;
	bool Busy;
	str Destination;
	str Temporary;
};

bool __xrtCsvCopyDialect(xrtcsvdialect* pOutput, const xrtcsvdialect* pInput);
void __xrtCsvSetReaderError(xrtcsvreader* pReader, int iCode, const char* sFormat, ...);
void __xrtCsvSetWriterError(xrtcsvwriter* pWriter, int iCode, const char* sFormat, ...);
bool __xrtCsvWriterWriteAll(xrtcsvwriter* pWriter, const void* pData, uint64 iSize);
bool __xrtCsvFailed(const xerror* pError);

#endif
