#ifndef XRT_CSV_H
#define XRT_CSV_H

/* CSV 使用 XRT 流、缓冲与严格 UTF-8 验证，不启用完整运行库。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
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
	uint64 iLine;
	uint64 iColumn;
	uint64 iOffset;
	char sMessage[256];
} xrtcsverror;

typedef struct {
	uint64 iRow;
	uint32 iCount;
	/* 行与字段只借用，下一次 Next、Close 或 Destroy 后失效。 */
	const char* const* pFields;
} xrtcsvrow;

typedef struct xrtcsvreader xrtcsvreader;
typedef struct xrtcsvwriter xrtcsvwriter;

/* 使用初始化函数后修改选项；分隔符允许 TAB/可打印 ASCII，引号只允许可打印 ASCII。
 * 限额为 0 表示取消该项上限；默认限额来自 xrtResourceLimitsInit。 */
XRT_API void xrtCsvDialectInit(xrtcsvdialect* pDialect);
XRT_API void xrtCsvErrorInit(xrtcsverror* pError);

/* Create 接管流时，构造失败也消费该流。借用流则不负责关闭。
 * 创建可返回带错误的非 NULL 对象，须检查 Error()->iCode，并最终 Destroy。 */
XRT_API xrtcsvreader* xrtCsvReaderCreate(xreader* pReader, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
/* 借用输入字节，不复制；输入必须保持不变并存活到读取器销毁。 */
XRT_API xrtcsvreader* xrtCsvReaderOpenMemory(const void* pData, uint64 iSize,
	const xrtcsvdialect* pDialect);
XRT_API xrtcsvreader* xrtCsvReaderOpenFile(cstr sPath, const xrtcsvdialect* pDialect);
XRT_API const xrtcsverror* xrtCsvReaderError(const xrtcsvreader* pReader);
/* false 可能是 EOF 或错误：同时检查 EOF、Error 及 xrtGetError。
 * IO/内存首错不会被后续诊断 OOM 覆盖；无法分配包装时直接发布底层错误，
 * CSV 自身的错误码和位置仍可通过 Error 查询，Writer 同理。 */
XRT_API bool xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow);
XRT_API bool xrtCsvReaderEOF(const xrtcsvreader* pReader);
XRT_API bool xrtCsvReaderClose(xrtcsvreader* pReader);
XRT_API void xrtCsvReaderDestroy(xrtcsvreader* pReader);

/* 流所有权和带错误对象的规则与 ReaderCreate 相同；外部流不能回滚已写字节。 */
XRT_API xrtcsvwriter* xrtCsvWriterCreate(xwriter* pWriter, bool bTakeOwnership,
	const xrtcsvdialect* pDialect);
XRT_API xrtcsvwriter* xrtCsvWriterCreateMemory(const xrtcsvdialect* pDialect);
/* 使用同目录临时文件，只有 Close 成功才替换目标；Destroy 不自动提交。 */
XRT_API xrtcsvwriter* xrtCsvWriterOpenFile(cstr sPath, const xrtcsvdialect* pDialect);
XRT_API const xrtcsverror* xrtCsvWriterError(const xrtcsvwriter* pWriter);
/* 字段为 NUL 结尾的 UTF-8，NULL 字段按空字符串处理；不保留输入。 */
XRT_API bool xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields);
XRT_API bool xrtCsvWriterClose(xrtcsvwriter* pWriter);
/* 仅限内存 Writer：先 Close，再取一次结果；调用者负责 xrtBufferDestroy。 */
XRT_API xbuffer* xrtCsvWriterTakeBuffer(xrtcsvwriter* pWriter);
XRT_API void xrtCsvWriterDestroy(xrtcsvwriter* pWriter);

#ifdef __cplusplus
}
#endif

#endif
