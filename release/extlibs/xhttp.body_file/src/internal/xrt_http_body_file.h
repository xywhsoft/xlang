#ifndef XRT_INTERNAL_HTTP_BODY_FILE_H
#define XRT_INTERNAL_HTTP_BODY_FILE_H

#include "xrt_http_body.h"
#include "../../include/xrt/http_body_file.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 异步文件区间 cursor 只管理一次连续区间，不拥有底层文件。 */
typedef struct xrt_http_body_file_cursor {
	xasyncfile* File;
	xfuture* Pending;
	uint64 Offset;
	uint64 Remaining;
	size_t ReadSize;
	size_t Requested;
	size_t ReadyOffset;
	bool Ready;
} xrt_http_body_file_cursor;



/* 静默关闭同步文件，并保留当前执行上下文正在处理的错误。 */
void __xrtHttpBodyFileCloseSync(xfile File);



/* 请求关闭异步文件，并保留当前执行上下文正在处理的错误。 */
void __xrtHttpBodyFileCloseAsync(xasyncfile* pFile);



/* 初始化一个尚未提交读取的连续文件区间。 */
void __xrtHttpBodyFileCursorInit(
	xrt_http_body_file_cursor* pCursor,
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	size_t iReadSize
);



/* 取消并释放 cursor 尚未消费完的读取 Future。 */
void __xrtHttpBodyFileCursorCancel(
	xrt_http_body_file_cursor* pCursor
);



/* 在 AGAIN 后返回当前读取 Future 的独立消费端引用。 */
xfuture* __xrtHttpBodyFileCursorWait(
	xrt_http_body_file_cursor* pCursor
);



/* 推进连续文件区间并发布拥有独立 Future 租约的 Chunk。 */
xhttpbodystatus __xrtHttpBodyFileCursorNext(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
);



/* 校验文件区间能由跨平台异步文件偏移表达。 */
bool __xrtHttpBodyFileRangeValid(
	uint64 iOffset,
	uint64 iLength
);

#endif

#endif

