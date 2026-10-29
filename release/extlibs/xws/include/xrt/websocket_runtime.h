#ifndef XWS_WEBSOCKET_RUNTIME_H
#define XWS_WEBSOCKET_RUNTIME_H

#include "../xws/features.h"
#include <xrt.h>



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION) && \
	(!defined(XRT_FEATURE_SPIN) || \
	 !defined(XRT_FEATURE_WEBSOCKET_MESSAGE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "xws connections require spin, messages, secure random and TCP"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF) && \
	!defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
	#error "xws connection references require connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER) && \
	!defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
	#error "xws writers require connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF) && \
	(!defined(XWS_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF))
	#error "xws writer references require writer and connection reference support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "xws TLS connections require connections and TLS streams"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATER))
	#error "xws compressed connections require connection, Inflater and Deflater"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE))
	#error "xws compressed writers require writer and compressed connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "xws connection Futures require connection and Future support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE))
	#error "xws reference Futures require reference and Future connection support"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

#define XWS_CONN_MESSAGE_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_FRAME_LIMIT_DEFAULT UINT64_C(1048576)
#define XWS_CONN_SEND_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_CONTROL_RESERVE_DEFAULT ((size_t)512u)
#define XWS_CONN_CLOSE_TIMEOUT_DEFAULT UINT64_C(5000)
#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
	#define XWS_CONN_ASYNC_BYTES_DEFAULT ((size_t)1048576u)
	#define XWS_CONN_ASYNC_COUNT_DEFAULT UINT32_C(1024)
	#define XWS_CONN_ASYNC_BATCH_DEFAULT UINT32_C(64)
#endif



/* 已建立会话只有开放、关闭握手和传输终态三个阶段。 */
typedef enum xwsconnstate {
	XWS_CONN_OPEN = 0,
	XWS_CONN_CLOSING,
	XWS_CONN_CLOSED
} xwsconnstate;



/* Connection 错误码区分协议、资源、发送和底层传输边界。 */
typedef enum xwsconnerror {
	XWS_CONN_ERROR_ARGUMENT = 1,
	XWS_CONN_ERROR_CONFIG,
	XWS_CONN_ERROR_MEMORY,
	XWS_CONN_ERROR_STATE,
	XWS_CONN_ERROR_FRAME,
	XWS_CONN_ERROR_MESSAGE,
	XWS_CONN_ERROR_RANDOM,
	XWS_CONN_ERROR_SEND,
	XWS_CONN_ERROR_LIMIT,
	XWS_CONN_ERROR_TRANSPORT,
	XWS_CONN_ERROR_TIMEOUT
} xwsconnerror;



/* Close 标志同时描述本地、远端和 RFC 6455 完整握手结果。 */
typedef enum xwsconncloseflag {
	XWS_CONN_CLOSE_SENT = UINT32_C(0x00000001),
	XWS_CONN_CLOSE_RECEIVED = UINT32_C(0x00000002),
	XWS_CONN_CLOSE_CLEAN = UINT32_C(0x00000004),
	XWS_CONN_CLOSE_REMOTE = UINT32_C(0x00000008)
} xwsconncloseflag;



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 条件 Future 不复制消息，只观察统一 Connection 状态。 */
typedef enum xwsconnwait {
	XWS_CONN_WAIT_WRITE = 1,
	XWS_CONN_WAIT_DRAIN,
	XWS_CONN_WAIT_CLOSE
} xwsconnwait;
#endif



/*
	MessageLimit 作用于扩展解码后的逻辑消息，FrameLimit 作用于线路帧。
	SendLimit 是 WebSocket 与底层传输待发字节的硬预算；TLS 会按记录密文
	线路长度计账。ControlReserve 从普通数据预算中保留三个最大控制帧槽：
	手动 Ping/Pong、自动 Pong、Close 逐层使用，低优先级发送不能占用
	更高优先级槽。启用 Future 层时，
	AsyncBytesLimit 和 AsyncCountLimit 独立限制尚未受理的异步操作，
	AsyncBatch 限制一次 Worker 轮转完成的操作数。
*/
typedef struct xwsconnconfig {
	xwsrole Role;
	xstrview Protocol;
	size_t MessageLimit;
	uint64 FrameLimit;
	size_t SendLimit;
	size_t ControlReserve;
	uint64 CloseTimeout;
	bool AutoPong;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		size_t AsyncBytesLimit;
		uint32 AsyncCountLimit;
		uint32 AsyncBatch;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xwsdeflate Deflate;
		xwsinflaterconfig Inflater;
		xwsdeflaterconfig Deflater;
		bool DeflateEnabled;
	#endif
} xwsconnconfig;



/* Reason 借用 Connection 内部不可变副本，至少保持到 Connection 销毁。 */
typedef struct xwsconnclose {
	uint32 Flags;
	xnetresult Transport;
	uint16 LocalCode;
	uint16 RemoteCode;
	xstrview Reason;
} xwsconnclose;



typedef struct xwsconn xwsconn;

#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* Writer 独占一条尚未结束的出站数据消息。 */
typedef struct xwswriter xwswriter;
#endif



/*
	消息事件按 Begin、零个或多个 Data、End 发布，支持空消息和流式消费。
	MessageBegin 的 Info 以及 Data、Ping、Pong 的视图仅在对应同步回调期间有效。
	Info 描述首个数据帧；COMPRESSED 表示 Connection 已经按协商扩展解码消息。
	全部事件都在传输所属 Worker 上串行执行。
*/
typedef struct xwsconnevents {
	void (*MessageBegin)(
		xwsconn* pConnection,
		const xwsmessageinfo* pInfo,
		ptr pData
	);
	void (*MessageData)(
		xwsconn* pConnection,
		xbytesview Data,
		ptr pData
	);
	void (*MessageEnd)(
		xwsconn* pConnection,
		ptr pData
	);
	void (*Ping)(
		xwsconn* pConnection,
		xbytesview Payload,
		ptr pData
	);
	void (*Pong)(
		xwsconn* pConnection,
		xbytesview Payload,
		ptr pData
	);
	void (*Backpressure)(
		xwsconn* pConnection,
		size_t iPending,
		ptr pData
	);
	void (*Writable)(
		xwsconn* pConnection,
		size_t iPending,
		ptr pData
	);
	void (*Drain)(
		xwsconn* pConnection,
		ptr pData
	);
	void (*Error)(
		xwsconn* pConnection,
		const xerror* pError,
		ptr pData
	);
	void (*Close)(
		xwsconn* pConnection,
		const xwsconnclose* pClose,
		ptr pData
	);
} xwsconnevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

/*
	初始化服务端角色、1 MiB 收发上限、512 字节控制预算和五秒关闭超时。
	pConfig 可以指向完整但未对齐的可写结构存储；失败时不修改该存储。
*/
XRT_API void xrtWsConnConfigInit(xwsconnconfig* pConfig);



/*
	无分配验证完整 Connection 配置；不会修改传入结构。
	pConfig 及其 Protocol 视图都必须是完整有效范围，结构本身可以未对齐。
*/
XRT_API bool xrtWsConnConfigValid(
	const xwsconnconfig* pConfig
);



/*
	在 TCP Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 Stream 引用，并在下一次 Worker 循环处理
	已经缓冲的早到帧；失败时 Stream 所有权保持不变。
	TCP WriteLimit 必须同时容纳 ControlReserve 和至少一个空数据帧。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XRT_API xwsconn* xrtWsConnAttach(
	xnetstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/*
	在 TLS Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 TLS Stream 引用，明文余量不会丢失。
	ControlReserve 和 SendLimit 必须容纳协商后 TLS 记录开销。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XRT_API xwsconn* xrtWsConnAttachTls(
	xtlsstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);
#endif



/* 增加 Connection 引用并返回原指针。 */
XRT_API xwsconn* xrtWsConnRef(xwsconn* pConnection);



/* 释放 Connection 引用；关闭或中止传输必须另行请求。 */
XRT_API void xrtWsConnDestroy(xwsconn* pConnection);



/* 返回并发可读的会话状态。 */
XRT_API xwsconnstate xrtWsConnState(const xwsconn* pConnection);



/* 返回建立连接时固定的本端角色。 */
XRT_API xwsrole xrtWsConnRole(const xwsconn* pConnection);



/* 返回 Connection 拥有的已协商子协议快照。 */
XRT_API xstrview xrtWsConnProtocol(
	const xwsconn* pConnection
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	复制协商后的 permessage-deflate 响应。
	返回 false 表示连接未启用压缩或参数无效。
	pDeflate 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出。
*/
XRT_API bool xrtWsConnDeflate(
	const xwsconn* pConnection,
	xwsdeflate* pDeflate
);
#endif



/* 返回传输所属的借用 Worker。 */
XRT_API xnetworker* xrtWsConnWorker(const xwsconn* pConnection);



/* 在所属 Worker 上借用 TCP Stream；TLS 会话返回空指针。 */
XRT_API xnetstream* xrtWsConnTcp(const xwsconn* pConnection);



/* 从任意线程取得 TCP Stream 强引用；调用方最终 Destroy。 */
XRT_API xnetstream* xrtWsConnTcpRef(const xwsconn* pConnection);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 在所属 Worker 上借用 TLS Stream；TCP 会话返回空指针。 */
XRT_API xtlsstream* xrtWsConnTls(const xwsconn* pConnection);



/* 从任意线程取得 TLS Stream 强引用；调用方最终 Destroy。 */
XRT_API xtlsstream* xrtWsConnTlsRef(const xwsconn* pConnection);
#endif



/* 返回 WebSocket 与底层传输当前待发字节的并发快照。 */
XRT_API size_t xrtWsConnPending(const xwsconn* pConnection);



/* 返回普通数据当前仍可受理的硬预算快照。 */
XRT_API size_t xrtWsConnWritable(const xwsconn* pConnection);



/*
	暂停应用消息读取。
	可以从任意线程调用；当前回调分块完成后不再发布后续消息事件。
*/
XRT_API void xrtWsConnPause(xwsconn* pConnection);



/*
	恢复应用消息读取并投递一次所属 Worker 驱动。
	已关闭连接返回 false；关闭握手中的连接允许恢复协议读取。
*/
XRT_API bool xrtWsConnResume(xwsconn* pConnection);



/* 返回接收侧当前是否被应用暂停的并发快照。 */
XRT_API bool xrtWsConnPaused(const xwsconn* pConnection);



/*
	在所属 Worker 上复制发送一条完整 Text 或 Binary 消息。
	Text 在受理前完成 UTF-8 校验；当前预算不足返回 AGAIN，
	单帧永久超过 WebSocket 或 TCP 硬容量返回 ERROR 和 XERR_RANGE。
	Payload 必须是完整且不发生地址回绕的只读范围，返回前已经完成复制。
*/
XRT_API xnetresult xrtWsConnSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnText(
	xwsconn* pConnection,
	xstrview Text
);



/* 发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinary(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/*
	在所属 Worker 上发送一条所有权消息。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但结构和数据必须是完整范围，且都不能覆盖 Connection。
	空负载不接管所有权，应改用普通 Send；失败不会调用 Release。
*/
XRT_API xnetresult xrtWsConnSendRef(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 发送一条所有权 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnTextRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 发送一条所有权 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinaryRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/*
	在所属 Worker 上发送并接管一段 xrtMalloc 内存。
	仅返回 OK 时由 Connection 最终 xrtFree；空负载应改用普通 Send。
*/
XRT_API xnetresult xrtWsConnSendTake(
	xwsconn* pConnection,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc UTF-8 Text。 */
XRT_API xnetresult xrtWsConnTextTake(
	xwsconn* pConnection,
	str sText,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc Binary。 */
XRT_API xnetresult xrtWsConnBinaryTake(
	xwsconn* pConnection,
	bytes pData,
	size_t iSize
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/*
	在所属 Worker 上开始一条未压缩的分片 Text 或 Binary 消息。
	同一 Connection 同时只能存在一个 Writer；失败返回空指针并设置错误。
*/
XRT_API xwswriter* xrtWsConnBegin(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条未压缩的分片 UTF-8 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginText(
	xwsconn* pConnection
);



/* 开始一条未压缩的分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinary(
	xwsconn* pConnection
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/*
	在所属 Worker 上开始一条压缩分片 Text 或 Binary 消息。
	每次 Write 都独立建立同步边界，因此成功返回后对应线路帧已被传输层受理。
	AGAIN、永久容量和 OOM 不推进 Writer；半条消息后的编码或传输故障会终止会话。
*/
XRT_API xwswriter* xrtWsConnBeginCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条压缩分片 UTF-8 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginTextCompressed(
	xwsconn* pConnection
);



/* 开始一条压缩分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinaryCompressed(
	xwsconn* pConnection
);
#endif



/*
	复制并提交一个非最终分片。
	仅返回 OK 时推进消息大小、UTF-8 和首帧状态；AGAIN 可原样重试。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XRT_API xnetresult xrtWsWriterWrite(
	xwswriter* pWriter,
	xbytesview Data
);



/*
	复制并提交最终分片；空分片合法。
	成功后释放 Connection 的 Writer 独占权，但对象仍需 Destroy。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XRT_API xnetresult xrtWsWriterFinish(
	xwswriter* pWriter,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
/*
	提交一个非空所有权分片。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XRT_API xnetresult xrtWsWriterWriteRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/*
	提交最终所有权分片；失败时 Ref 所有权保持不变。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XRT_API xnetresult xrtWsWriterFinishRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/* 提交并接管一个非空 xrtMalloc 分片。 */
XRT_API xnetresult xrtWsWriterWriteTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);



/* 提交并接管一个非空 xrtMalloc 最终分片。 */
XRT_API xnetresult xrtWsWriterFinishTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);
#endif



/* 返回 Writer 是否已经成功提交最终分片；空指针返回 false，回绕范围设置参数错误。 */
XRT_API bool xrtWsWriterIsFinished(
	const xwswriter* pWriter
);



/*
	在所属 Worker 上销毁 Writer。
	放弃已经开始但未结束的消息会尝试发送 1011 Close，失败则中止传输。
	空指针无操作；回绕对象范围只设置参数错误。
*/
XRT_API void xrtWsWriterDestroy(
	xwswriter* pWriter
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	压缩并发送一条完整 Text 或 Binary 消息。
	失败不会让未发送的压缩上下文影响下一条消息。
*/
XRT_API xnetresult xrtWsConnSendCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 压缩并发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnTextCompressed(
	xwsconn* pConnection,
	xstrview Text
);



/* 压缩并发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinaryCompressed(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/*
	发送 Ping 控制帧；Payload 最多 125 字节。
	控制帧可以使用普通数据不可占用的预留预算。
*/
XRT_API xnetresult xrtWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Pong 控制帧；通常在 AutoPong 关闭后的 Ping 回调中使用。
	Payload 最多 125 字节，并使用控制帧预留预算。
*/
XRT_API xnetresult xrtWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Close 并进入关闭握手；Code 为零且 Reason 为空时发送空负载。
	重复调用返回 CLOSED，不会发送第二个 Close。
*/
XRT_API xnetresult xrtWsConnClose(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);



/*
	从任意线程请求立即异常关闭底层传输。
	OPEN 只会前进到 CLOSING；已经 CLOSED 时返回 false 且终态保持不变。
*/
XRT_API bool xrtWsConnAbort(xwsconn* pConnection);



/*
	一次性复制当前 Close 快照；Reason 仍借用 Connection 内部存储。
	pClose 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出，成功后除 Reason 视图外不再借用 Connection 状态。
*/
XRT_API bool xrtWsConnCloseInfo(
	const xwsconn* pConnection,
	xwsconnclose* pClose
);



/* 返回 Connection 第一个结构化错误；结果借用到 Connection 销毁。 */
XRT_API const xerror* xrtWsConnError(const xwsconn* pConnection);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 返回尚未由 Connection Worker 终结的异步发送负载字节数。 */
XRT_API size_t xrtWsConnAsyncBytes(const xwsconn* pConnection);



/* 返回异步发送和条件等待的合计操作数。 */
XRT_API uint32 xrtWsConnAsyncCount(const xwsconn* pConnection);



/*
	建立 WRITE、DRAIN 或 CLOSE 条件 Future。
	WRITE 与 DRAIN 是发送 FIFO 屏障，只等待调用前已经排队的发送；
	WRITE 随后要求当前可写，DRAIN 随后要求传输待发归零。
	取消只移除本次等待，不会关闭 Connection。
*/
XRT_API xfuture* xrtWsConnWaitAsync(
	xwsconn* pConnection,
	xwsconnwait Wait
);



/*
	从任意线程复制并按 FIFO 提交一条完整消息。
	Future 在帧被传输层受理时完成；排空必须另行等待 DRAIN。
	取消只在 Worker 受理前有效，不会撤回已经提交的线路帧。
*/
XRT_API xfuture* xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryAsync(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
/*
	从任意线程异步提交一条所有权消息。
	成功返回 Future 时立即接管 Ref；终态、取消和关闭都恰好调用一次 Release。
	空负载不接管所有权，应改用普通 Async 发送。
	Ref 可以未对齐，但结构与数据都必须是完整、不回绕且与 Connection 分离的范围；
	任何预检失败都不接管所有权，也不会调用 Release。
*/
XRT_API xfuture* xrtWsConnSendRefAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 Binary。 */
XRT_API xfuture* xrtWsConnBinaryRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 从任意线程复制、压缩并异步提交一条完整消息。 */
XRT_API xfuture* xrtWsConnSendCompressedAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制、压缩并异步提交 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextCompressedAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制、压缩并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryCompressedAsync(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/* 从任意线程复制并异步提交 Ping。 */
XRT_API xfuture* xrtWsConnPingAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 Pong。 */
XRT_API xfuture* xrtWsConnPongAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程异步提交唯一 Close。 */
XRT_API xfuture* xrtWsConnCloseAsync(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);
#endif

#endif



XRT_EXTERN_C_END

#endif
