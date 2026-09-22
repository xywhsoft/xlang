#ifndef XRT_CSV_H
#define XRT_CSV_H

/* CSV 公共契约直接依赖连续缓冲和通用 Reader/Writer。 */
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

XRT_API void xrtCsvDialectInit(xrtcsvdialect* pDialect);
XRT_API void xrtCsvErrorInit(xrtcsverror* pError);

XRT_API xrtcsvreader* xrtCsvReaderCreate(xreader* pReader, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
XRT_API xrtcsvreader* xrtCsvReaderOpenMemory(const void* pData, uint64 iSize,
	const xrtcsvdialect* pDialect);
XRT_API xrtcsvreader* xrtCsvReaderOpenFile(str sPath, const xrtcsvdialect* pDialect);
XRT_API const xrtcsverror* xrtCsvReaderError(const xrtcsvreader* pReader);
XRT_API bool xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow);
XRT_API bool xrtCsvReaderEOF(const xrtcsvreader* pReader);
XRT_API void xrtCsvReaderDestroy(xrtcsvreader* pReader);

XRT_API xrtcsvwriter* xrtCsvWriterCreate(xwriter* pWriter, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
XRT_API xrtcsvwriter* xrtCsvWriterCreateMemory(const xrtcsvdialect* pDialect);
XRT_API xrtcsvwriter* xrtCsvWriterOpenFile(str sPath, const xrtcsvdialect* pDialect);
XRT_API const xrtcsverror* xrtCsvWriterError(const xrtcsvwriter* pWriter);
XRT_API bool xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields);
XRT_API bool xrtCsvWriterClose(xrtcsvwriter* pWriter);
XRT_API xbuffer* xrtCsvWriterTakeBuffer(xrtcsvwriter* pWriter);
XRT_API void xrtCsvWriterDestroy(xrtcsvwriter* pWriter);

#ifdef __cplusplus
}
#endif

#endif
