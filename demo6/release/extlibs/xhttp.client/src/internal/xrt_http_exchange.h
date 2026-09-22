#ifndef XRT_INTERNAL_HTTP_EXCHANGE_H
#define XRT_INTERNAL_HTTP_EXCHANGE_H

#include "xrt_http_client.h"




#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)

/* 设置 Exchange 实现使用的通用参数错误。 */
void __xrtHttp1ExchangeInvalidArgument(void);



/* 设置 Exchange 实现使用的非法状态错误。 */
void __xrtHttp1ExchangeInvalidState(void);



/* 设置 Exchange 实现使用的大小溢出错误。 */
void __xrtHttp1ExchangeSizeOverflow(void);



/* 临时字节缓冲首次按实际输入分配，后续才按需倍增。 */
typedef struct xrt_http_exchange_buffer {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xrt_http_exchange_buffer;



/* 出站片段状态把 Header、chunk 元数据和正文租约分开推进。 */
typedef enum xrt_http_exchange_output {
	XRT_HTTP_EXCHANGE_OUTPUT_HEAD = 0,
	XRT_HTTP_EXCHANGE_OUTPUT_BODY,
	XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_LINE,
	XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_DATA,
	XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_END,
	XRT_HTTP_EXCHANGE_OUTPUT_DONE
} xrt_http_exchange_output;



/* 入站只在最终响应 Header 后进入 Body 或终态。 */
typedef enum xrt_http_exchange_input {
	XRT_HTTP_EXCHANGE_INPUT_HEAD = 0,
	XRT_HTTP_EXCHANGE_INPUT_BODY,
	XRT_HTTP_EXCHANGE_INPUT_DONE,
	XRT_HTTP_EXCHANGE_INPUT_UPGRADED,
	XRT_HTTP_EXCHANGE_INPUT_FAILED
} xrt_http_exchange_input;



/* Exchange 对象没有固定大缓冲，所有协议输入存储都按需建立。 */
struct xhttp1exchange {
	xhttp1requestplan* Plan;
	xhttp1exchangeconfig Config;
	xhttp1exchangeevents Events;
	xerror* Error;
	xhttpresponse* Response;

	xrt_http_exchange_output OutputState;
	xhttpbodyreader* Reader;
	xhttpbodychunk Chunk;
	uint64 BodyRemaining;
	uint64 RequestWireBytes;
	size_t HeadOffset;
	size_t PartOffset;
	size_t Offered;
	cbytes OfferData;
	char ChunkLine[32];
	size_t ChunkLineSize;
	bool ContinueAllowed;
	bool OutputAgain;
	bool OutputStopped;
	bool RequestComplete;
	bool ReaderOpened;
	bool ChunkTerminal;

	xrt_http_exchange_input InputState;
	xrt_http_exchange_buffer HeadBuffer;
	xrt_http_exchange_buffer Pending;
	uint32 HeadDelimiter;
	xhttpfield* Fields;
	size_t FieldCapacity;
	xhttpfield* Trailers;
	size_t TrailerCapacity;
	xhttp1body Body;
	xhttp1bodyplan BodyPlan;
	uint32 ResponseFlags;
	uint32 InformationalCount;
	bool ResponseComplete;
	bool Upgraded;
	bool TransportEnded;
	bool Paused;
};



/* 验证 Exchange 解析、存储和信息响应限额。 */
bool __xrtHttp1ExchangeConfigValid(
	const xhttp1exchangeconfig* pConfig
);



/* 发布一个拥有原因链的稳定 Exchange 终态错误。 */
bool __xrtHttp1ExchangeFail(
	xhttp1exchange* pExchange,
	xhttp1exchangeerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 包装一个明确来自本次下层调用的错误原因。 */
bool __xrtHttp1ExchangeFailCause(
	xhttp1exchange* pExchange,
	xhttp1exchangeerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 停止出站状态机并释放尚未发送的正文租约与 Reader。 */
void __xrtHttp1ExchangeStopOutput(
	xhttp1exchange* pExchange
);



/* 按硬上限追加临时输入。 */
bool __xrtHttp1ExchangeBufferAppend(
	xhttp1exchange* pExchange,
	xrt_http_exchange_buffer* pBuffer,
	xbytesview Data,
	size_t iLimit
);



/* 从临时输入头部移除一段已经同步处理的数据。 */
void __xrtHttp1ExchangeBufferConsume(
	xrt_http_exchange_buffer* pBuffer,
	size_t iSize
);

#endif

#endif
