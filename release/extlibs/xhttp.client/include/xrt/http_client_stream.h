#ifndef XRT_HTTP_CLIENT_STREAM_H
#define XRT_HTTP_CLIENT_STREAM_H




#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) && \
	(!defined(XHTTP_FEATURE_HTTP_EXCHANGE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XRT HTTP client stream support requires HTTP exchange and TCP support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT HTTPS client stream support requires HTTP client stream and TLS stream support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC))
	#error "XRT async HTTP client stream support requires stream and async exchange support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

/* 单次 HTTP/1 调用在已打开的 TCP 或 TLS Stream 上驱动一个 Exchange。 */
typedef struct xhttp1call xhttp1call;



/* 调用状态只从 RUNNING 进入一个不可变终态。 */
typedef enum xhttp1callstate {
	XHTTP1_CALL_RUNNING = 0,
	XHTTP1_CALL_SUCCEEDED,
	XHTTP1_CALL_FAILED,
	XHTTP1_CALL_CANCELLED
} xhttp1callstate;



/* 调用错误位于 xrt.http.call 域，区分状态、传输和 Exchange 失败。 */
typedef enum xhttp1callerror {
	XHTTP1_CALL_ERROR_ARGUMENT = 1,
	XHTTP1_CALL_ERROR_STATE,
	XHTTP1_CALL_ERROR_TRANSPORT,
	XHTTP1_CALL_ERROR_EXCHANGE,
	XHTTP1_CALL_ERROR_CANCELLED
} xhttp1callerror;



/* Progress 只在真实接受输出、消费输入或完整发送请求时产生。 */
typedef enum xhttp1progress {
	XHTTP1_PROGRESS_WRITE = 1,
	XHTTP1_PROGRESS_READ,
	XHTTP1_PROGRESS_REQUEST_DONE
} xhttp1progress;



/* WriteSize 只限制单次借出和发送，不建立固定缓冲。 */
typedef struct xhttp1callconfig {
	size_t WriteSize;
} xhttp1callconfig;



/*
	完成结果只在回调期间借用。成功时 Response 非空；失败时正文和传输均为空。
	Reusable 与 Upgraded 互斥，任一为真时 Tcp/Tls 中恰有一个调用方引用转移给回调。
	Reusable 时 Buffered 为零；Buffered 只表示 Upgrade 后仍留在传输中的协议外字节。
*/
typedef struct xhttp1callresult {
	xnetresult Result;
	xhttpresponse* Response;
	xnetstream* Tcp;
	xtlsstream* Tls;
	const xerror* Error;
	size_t Buffered;
	bool Reusable;
	bool Upgraded;
} xhttp1callresult;



/*
	完成回调在底层 Stream 所属 Worker 上同步执行且至多一次。
	构造函数成功返回之前不会执行完成回调。
*/
typedef void (*xhttp1callproc)(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
);



/*
	进度回调在 Stream 所属 Worker 上同步执行。
	Bytes 只对 WRITE 和 READ 有效，REQUEST_DONE 的 Bytes 为零且至多出现一次。
*/
typedef void (*xhttp1progressproc)(
	xhttp1call* pCall,
	xhttp1progress Progress,
	size_t iBytes,
	ptr pData
);



/* Done 必须非空；Progress 为空时调用热路径没有额外回调。 */
typedef struct xhttp1callevents {
	xhttp1callproc Done;
	xhttp1progressproc Progress;
	ptr Data;
} xhttp1callevents;



XRT_EXTERN_C_BEGIN



/*
	初始化 16 KiB 单次输出上限；不会预分配对应大小的缓冲。
	配置允许位于未对齐存储，Call 创建时会先复制完整快照。
*/
XRT_API void xrtHttp1CallConfigInit(xhttp1callconfig* pConfig);



/*
	初始化空进度观察器；调用方必须随后设置 Done。
	事件表允许位于未对齐存储，Call 创建时会先复制完整快照。
*/
XRT_API void xrtHttp1CallEventsInit(xhttp1callevents* pEvents);



/*
	接管一个已打开 TCP Stream 调用方引用和一个 Exchange。
	必须在 Stream 所属 Worker 上调用；失败时两个输入仍归调用方。
*/
XRT_API xhttp1call* xrtHttp1CallTcp(
	xnetstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/*
	接管一个已完成握手的 TLS Stream 调用方引用和一个 Exchange。
	必须在 Stream 所属 Worker 上调用；失败时两个输入仍归调用方。
*/
XRT_API xhttp1call* xrtHttp1CallTls(
	xtlsstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
);

#endif



/* 增加调用引用并返回原指针。 */
XRT_API xhttp1call* xrtHttp1CallRef(xhttp1call* pCall);



/* 释放调用引用；空指针视为空操作。 */
XRT_API void xrtHttp1CallDestroy(xhttp1call* pCall);



/*
	从任意线程协作取消调用并异常关闭尚未交出的传输。
	返回 true 表示取消已被接纳，最终状态必为 CANCELLED；
	返回 false 表示已有取消或终态已经提交。
*/
XRT_API bool xrtHttp1CallCancel(xhttp1call* pCall);



/* 在所属 Worker 上立即暂停响应输入和底层传输读取；重复调用安全。 */
XRT_API bool xrtHttp1CallPause(xhttp1call* pCall);



/* 从任意线程提交恢复；返回 false 表示未暂停、已提交恢复或已有终态。 */
XRT_API bool xrtHttp1CallResume(xhttp1call* pCall);



/* 返回响应输入暂停门的并发快照；调用进入任意终态后始终返回 false。 */
XRT_API bool xrtHttp1CallPaused(const xhttp1call* pCall);



/* 返回调用状态的并发快照。 */
XRT_API xhttp1callstate xrtHttp1CallState(const xhttp1call* pCall);



/* 返回失败或取消终态的借用错误；其他状态返回空。 */
XRT_API const xerror* xrtHttp1CallError(const xhttp1call* pCall);



XRT_EXTERN_C_END

#endif

#endif
