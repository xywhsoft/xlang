#ifndef XRT_INTERNAL_HTTP_CLIENT_STREAM_H
#define XRT_INTERNAL_HTTP_CLIENT_STREAM_H

#include "../../include/xhttp_client_package.h"

#include "xrt_http_exchange.h"
#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#endif


#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

/* 传输种类只用于选择类型安全的公开 Stream 操作。 */
typedef enum xrt_http1_call_transport {
	XRT_HTTP1_CALL_TCP = 0,
	XRT_HTTP1_CALL_TLS
} xrt_http1_call_transport;



/*
	调用由调用方、运行阶段和临时驱动过程分别持有引用。
	Lock 只线性化终态、取消和传输所有权，不进入协议与 I/O 热循环。
*/
struct xhttp1call {
	volatile int32 References;
	xspinlock Lock;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 PauseGate;
	xatomic32 ResumeGate;
	xatomicptr Stream;
	xnetworker* Worker;
	xhttp1exchange* Exchange;
	xhttp1callconfig Config;
	xhttp1callevents Events;
	xerror* Error;
	xrt_http1_call_transport Transport;
	xnetpost StartPost;
	xnetpost ResumePost;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		xfuturewatch OutputWatch;
		xnetpost OutputPost;
		xfuture* OutputFuture;
		bool OutputNotified;
	#endif
	bool RuntimeHeld;
	bool Driving;
	bool DriveAgain;
	bool OutputWaiting;
	bool OutputQueued;
	size_t OutputBytes;
	bool InputEnded;
	bool RequestDoneObserved;
};



/* 验证调用配置不会造成零进展输出。 */
bool __xrtHttp1CallConfigValid(
	const xhttp1callconfig* pConfig
);




/* 判断普通 TCP 传输是否仍满足 HTTP/1 连接复用边界。 */
bool __xrtHttp1TcpReusable(const xnetstream* pStream);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 在所属 Worker 上判断 TLS 传输是否仍满足 HTTP/1 连接复用边界。 */
bool __xrtHttp1TlsReusable(xtlsstream* pStream);

#endif



/* 串行推进一次调用的收发状态机。 */
void __xrtHttp1CallDrive(xhttp1call* pCall);



/* 摘除传输并以结构化原因结束调用。 */
void __xrtHttp1CallFail(
	xhttp1call* pCall,
	xhttp1callerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)

/* 订阅 Exchange 当前正文背压 Future。 */
bool __xrtHttp1CallAsyncWait(xhttp1call* pCall);



/* 取消并摘除尚未完成的正文背压等待。 */
void __xrtHttp1CallAsyncStop(xhttp1call* pCall);

#endif

#endif

#endif
