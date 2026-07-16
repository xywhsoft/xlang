#ifndef XRT_CSV_H
#define XRT_CSV_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_CSV_DIALECT_VERSION 1u

typedef enum {
	XRT_CSV_OK = 0,
	XRT_CSV_ERROR_ARGUMENT = 1,
	XRT_CSV_ERROR_MEMORY = 2,
	XRT_CSV_ERROR_FORMAT = 3,
	XRT_CSV_ERROR_IO = 4,
	XRT_CSV_ERROR_LIMIT = 5,
	XRT_CSV_ERROR_STATE = 6
} xrtcsvstatus;

typedef enum {
	XRT_CSV_NEWLINE_LF = 0,
	XRT_CSV_NEWLINE_CRLF = 1
} xrtcsvnewline;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	char iDelimiter;
	char iQuote;
	int iNewline;
	bool bStrict;
	bool bQuoteAll;
	bool bWriteBOM;
	bool bReserved;
	xrtresourcelimits tLimits;
} xrtcsvdialect;

typedef struct {
	int iCode;
	uint64 iRow;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtcsverror;

typedef struct {
	uint64 iRow;
	uint32 iCount;
	const char* const* pFields;
} xrtcsvrow;

typedef struct xrtcsvreader xrtcsvreader;
typedef struct xrtcsvwriter xrtcsvwriter;

XXAPI void xrtCsvDialectInit(xrtcsvdialect* pDialect);
XXAPI void xrtCsvErrorInit(xrtcsverror* pError);

XXAPI xrtcsvreader* xrtCsvReaderCreate(xreader pReader, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
XXAPI xrtcsvreader* xrtCsvReaderOpenMemory(const void* pData, uint64 iSize,
	const xrtcsvdialect* pDialect);
XXAPI xrtcsvreader* xrtCsvReaderOpenFile(str sPath, const xrtcsvdialect* pDialect);
XXAPI const xrtcsverror* xrtCsvReaderError(const xrtcsvreader* pReader);
XXAPI bool xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow);
XXAPI bool xrtCsvReaderEOF(const xrtcsvreader* pReader);
XXAPI void xrtCsvReaderDestroy(xrtcsvreader* pReader);

XXAPI xrtcsvwriter* xrtCsvWriterCreate(xwriter pWriter, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
XXAPI xrtcsvwriter* xrtCsvWriterCreateMemory(const xrtcsvdialect* pDialect);
XXAPI xrtcsvwriter* xrtCsvWriterOpenFile(str sPath, const xrtcsvdialect* pDialect);
XXAPI const xrtcsverror* xrtCsvWriterError(const xrtcsvwriter* pWriter);
XXAPI bool xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields);
XXAPI bool xrtCsvWriterClose(xrtcsvwriter* pWriter);
XXAPI xbuffer xrtCsvWriterTakeBuffer(xrtcsvwriter* pWriter);
XXAPI void xrtCsvWriterDestroy(xrtcsvwriter* pWriter);

#ifdef __cplusplus
}
#endif

#endif
