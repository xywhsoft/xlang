#ifndef XRT_HTTP_BODY_STREAM_H
#define XRT_HTTP_BODY_STREAM_H

#include "http_body.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_ASYNC) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP body stream requires async body and mutex support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)

/* 默认预算同时约束载荷内存和极小 Chunk 的描述符数量。 */
#define XHTTP_BODY_STREAM_BYTES_DEFAULT ((size_t)1048576u)
#define XHTTP_BODY_STREAM_CHUNKS_DEFAULT ((size_t)256u)



/* 写入结果区分永久失败、正常关闭、已受理和暂时背压。 */
typedef enum xhttpbodystreamresult {
	XHTTP_BODY_STREAM_ERROR = -1,
	XHTTP_BODY_STREAM_CLOSED = 0,
	XHTTP_BODY_STREAM_OK = 1,
	XHTTP_BODY_STREAM_AGAIN = 2
} xhttpbodystreamresult;



/* 稳定错误码保留参数、配置、预算、状态和生产失败边界。 */
typedef enum xhttpbodystreamerror {
	XHTTP_BODY_STREAM_ERROR_ARGUMENT = 1,
	XHTTP_BODY_STREAM_ERROR_CONFIG,
	XHTTP_BODY_STREAM_ERROR_LIMIT,
	XHTTP_BODY_STREAM_ERROR_STATE,
	XHTTP_BODY_STREAM_ERROR_FAILED,
	XHTTP_BODY_STREAM_ERROR_INTERNAL
} xhttpbodystreamerror;



/* Stream 是并发生产端，生成的 Body 是唯一异步消费端。 */
typedef struct xhttpbodystream xhttpbodystream;



/* 非零硬预算包含队列和正在传输但尚未释放的 Chunk 租约。 */
typedef struct xhttpbodystreamconfig {
	size_t MaxBytes;
	size_t MaxChunks;
} xhttpbodystreamconfig;



/* Info 是不暴露队列节点和借用数据的并发快照。 */
typedef struct xhttpbodystreaminfo {
	size_t PendingBytes;
	size_t PendingChunks;
	uint64 WrittenBytes;
	uint64 ReadBytes;
	bool Opened;
	bool InputClosed;
	bool ConsumerClosed;
	bool Failed;
} xhttpbodystreaminfo;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)

/*
	初始化 1 MiB、256 Chunk 的有界生产流默认配置。
	配置对象只要求位于完整可访问的存储范围，不要求自然对齐。
*/
XRT_API void xrtHttpBodyStreamConfigInit(
	xhttpbodystreamconfig* pConfig
);



/* 无分配验证两个预算都非零；配置输入可以未对齐。 */
XRT_API bool xrtHttpBodyStreamConfigValid(
	const xhttpbodystreamconfig* pConfig
);



/*
	创建未知长度一次性 Body；配置被立即快照，输出句柄可以未对齐，
	但输出不得与非空配置重叠。
*/
XRT_API xhttpbody* xrtHttpBodyStreamCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
);



/* 增加生产端引用；最后一个生产端释放会自动发布 EOF。 */
XRT_API xhttpbodystream* xrtHttpBodyStreamRef(
	xhttpbodystream* pStream
);



/* 释放生产端引用；空指针安全，自动 EOF 不改变调用线程错误。 */
XRT_API void xrtHttpBodyStreamDestroy(
	xhttpbodystream* pStream
);



/* 复制有效且不覆盖 Stream 的非空字节；非 OK 时不保留输入。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWrite(
	xhttpbodystream* pStream,
	xbytesview Data
);



/* OK 时接管有效独立字节租约；其他结果仍由调用方释放。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWriteRef(
	xhttpbodystream* pStream,
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
);



/* OK 时接管有效独立的 xrtMalloc 数据；其他结果不接管。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWriteTake(
	xhttpbodystream* pStream,
	ptr pData,
	size_t iSize
);



/*
	返回下一次可能写入的共享代际 Future；调用方只释放自己的引用。
	一次写入返回 AGAIN 后，当前背压代际至少等待一次预算释放，避免残余空间
	不足以容纳原 Chunk 时立即完成并形成忙重试；完成后仍须重新尝试写入。
*/
XRT_API xfuture* xrtHttpBodyStreamWaitWritable(
	xhttpbodystream* pStream
);



/* 幂等发布正常 EOF；已排队 Chunk 仍按顺序完整交付。 */
XRT_API bool xrtHttpBodyStreamClose(
	xhttpbodystream* pStream
);



/* 以指定 Cause 永久失败，丢弃尚未交付的排队 Chunk。 */
XRT_API bool xrtHttpBodyStreamFail(
	xhttpbodystream* pStream,
	const xerror* pError
);



/*
	原子复制并发快照；输出可以未对齐，但必须与 Stream 内部存储完全分离。
	输出范围无效时失败且不写入部分结果。
*/
XRT_API bool xrtHttpBodyStreamInfo(
	const xhttpbodystream* pStream,
	xhttpbodystreaminfo* pInfo
);

#endif



XRT_EXTERN_C_END

#endif

