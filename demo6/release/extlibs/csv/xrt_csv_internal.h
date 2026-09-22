#ifndef XRT_CSV_INTERNAL_H
#define XRT_CSV_INTERNAL_H

#define XRT_MODULE_ALL
#include "xrt.h"
#include "include/xrt_csv.h"

struct xrtcsvreader {
	xreader* Reader;
	bool TakeReader;
	xrtcsvdialect Dialect;
	xrtcsverror Error;
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
	unsigned char Prefix[3];
	uint32 PrefixIndex;
	uint32 PrefixCount;
	int Pending;
	bool PrefixReady;
	bool EOFReached;
	bool Failed;
};

struct xrtcsvwriter {
	xwriter* Writer;
	bool TakeWriter;
	xbuffer* Memory;
	xrtcsvdialect Dialect;
	xrtcsverror Error;
	uint64 RowNumber;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 TotalFields;
	bool Closed;
	bool Failed;
};

bool __xrtCsvCopyDialect(xrtcsvdialect* pOutput, const xrtcsvdialect* pInput);
void __xrtCsvSetReaderError(xrtcsvreader* pReader, int iCode, const char* sFormat, ...);
void __xrtCsvSetWriterError(xrtcsvwriter* pWriter, int iCode, const char* sFormat, ...);
bool __xrtCsvWriterWriteAll(xrtcsvwriter* pWriter, const void* pData, uint64 iSize);

#endif
