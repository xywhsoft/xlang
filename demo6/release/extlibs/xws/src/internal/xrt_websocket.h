#ifndef XWS_INTERNAL_WEBSOCKET_H
#define XWS_INTERNAL_WEBSOCKET_H

#ifndef XWS_MODULE_WEBSOCKET_WRITER_DEFLATE
	#define XWS_MODULE_WEBSOCKET_WRITER_DEFLATE
#endif
#ifndef XWS_MODULE_WEBSOCKET_WRITER_REF
	#define XWS_MODULE_WEBSOCKET_WRITER_REF
#endif
#ifndef XWS_MODULE_WEBSOCKET_GROUP_FUTURE
	#define XWS_MODULE_WEBSOCKET_GROUP_FUTURE
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_TLS
	#define XWS_MODULE_WEBSOCKET_CONNECTION_TLS
#endif

#include "../../include/xws/features.h"
#include <string.h>

#include <xrt.h>
#include "../../include/xrt/websocket_runtime.h"



/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 取走当前原因链，并用扩展库稳定域包装后重新安装。 */
static inline void __xwsErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 设置扩展库公共参数错误。 */
static inline void __xwsErrorSetInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT,
		"xws",
		0,
		"invalid argument"
	);
}



/* 设置扩展库公共大小溢出错误。 */
static inline void __xwsErrorSetSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE,
		"xws",
		0,
		"size overflow"
	);
}



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 设置 WebSocket 握手层的结构化错误。 */
static inline void __xwsHandshakeError(
	xerrkind Kind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsErrorSetDetail(
		Kind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 将当前底层错误包装为稳定的 WebSocket 握手错误。 */
static inline void __xwsHandshakeWrap(
	xerrkind DefaultKind,
	xwshandshakeerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.handshake",
		(int32)Code,
		sOperation,
		sMessage
	);
}

#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

typedef struct __xrt_ws_async __xrt_ws_async;



/*
	发送类别按协议存活优先级逐层使用预算：数据、手动控制、自动 Pong、Close。
	较低优先级永远不能占用较高优先级的固定帧槽。
*/
typedef enum __xrt_ws_send_class {
	__XRT_WS_SEND_DATA = 0,
	__XRT_WS_SEND_CONTROL,
	__XRT_WS_SEND_AUTO_PONG,
	__XRT_WS_SEND_CLOSE
} __xrt_ws_send_class;



/* 验证 Connection 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsConnRangeValid(const xwsconn* pConnection);



/* 复用同步发送的完整消息参数检查，不触碰 Worker 状态。 */
bool __xrtWsConnMessageCheck(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
);



/* 验证活动操作位于 Connection 所属 Worker。 */
bool __xrtWsConnWorker(
	xwsconn* pConnection,
	cstr sOperation
);



/* 验证 Connection 及其协商协议副本占用的完整连续存储。 */
bool __xrtWsConnStorageRange(
	const xwsconn* pConnection,
	size_t* pSize
);



/* 记录普通数据第一次耗尽硬预算的背压边沿。 */
void __xrtWsConnBackpressure(xwsconn* pConnection);



/* 归一化底层发送失败，并按传输状态决定是否占用终态错误槽。 */
void __xrtWsConnSendFailure(
	xwsconn* pConnection,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
);



/* 检查一帧的协议上限、永久容量和当前发送预算。 */
xnetresult __xrtWsConnFrameBudget(
	xwsconn* pConnection,
	size_t iPayload,
	__xrt_ws_send_class Class,
	size_t* pWireSize
);



/* 复制并提交一帧，供完整消息、控制帧和 Writer 共享。 */
xnetresult __xrtWsConnSendFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xrt_ws_send_class Class,
	bool bCompressed
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/* 预留最终线路帧后推进并提交一个压缩 Writer 分片。 */
xnetresult __xrtWsConnSendDeflatePart(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Input,
	bool bFirst,
	bool bFinal
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/* 提交一个所有权帧；仅 OK 接管 Ref。 */
xnetresult __xrtWsConnSendRefFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xnetref Ref,
	bool bFinal
);



/* 释放由 Take 系列 API 接管的 XRT 内存。 */
void __xrtWsConnTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
);
#endif



/* 创建 Connection 域错误，供基础层和可裁剪适配层共享。 */
xerror* __xrtWsConnErrorCreate(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 设置不占用 Connection 终态错误槽的一次调用错误。 */
const xerror* __xrtWsConnReject(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 在所属 Worker 上推进异步发送和条件 Future。 */
void __xrtWsConnFutureNotify(xwsconn* pConnection);
#endif



typedef enum __xrt_ws_transport {
	__XRT_WS_TRANSPORT_TCP = 1,
	__XRT_WS_TRANSPORT_TLS
} __xrt_ws_transport;



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* Writer 只保存一条出站消息的事务状态，由 Connection Worker 串行访问。 */
struct xwswriter {
	xwsconn* Connection;
	xutf8state Utf8;
	size_t Size;
	xwsopcode Opcode;
	bool Started;
	bool Finished;
	bool Calling;
	bool DestroyRequested;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		bool Compressed;
	#endif
};



/* 创建并独占一条普通或压缩数据消息。 */
xwswriter* __xrtWsWriterCreate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	bool bCompressed
);
#endif



/* TLS 短写后只保留尚未进入 TLS 会话的精确帧余量。 */
typedef struct __xrt_ws_output {
	struct __xrt_ws_output* Next;
	size_t Size;
	size_t Offset;
	size_t Pending;
	uint8 Data[];
} __xrt_ws_output;



/*
	客户端与服务端共享同一个已建立会话状态。
	对象不持有固定收发缓冲；帧头和掩码使用栈空间，TLS 只在短写后保留
	按实际帧大小分配的余量节点。
*/
struct xwsconn {
	volatile int32 References;
	xatomic32 State;
	xatomic32 TransportResult;
	xatomicptr Transport;
	/* 同步传输强引用与跨线程 Close 快照。 */
	xspinlock TransportLock;
	xnetworker* Worker;
	__xrt_ws_transport TransportKind;
	xwsconnconfig Config;
	size_t SendOverhead;
	size_t ControlSlot;
	xstrview Protocol;
	xwsconnevents Events;
	ptr Data;
	xwsmessagestate Message;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xwswriter WriterStorage;
		xwswriter* Writer;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xwsinflater* Inflater;
		xwsdeflater* Deflater;
	#endif
	xwsframe Frame;
	xwsmessageinfo MessageInfo;
	uint64 FrameRemaining;
	uint64 FrameOffset;
	size_t ControlSize;
	uint8 Control[XWS_CLOSE_PAYLOAD_MAX];
	uint16 LocalCode;
	uint16 RemoteCode;
	uint16 FailureCloseCode;
	uint16 RemoteReasonSize;
	char RemoteReason[XWS_CLOSE_REASON_MAX + 1u];
	xatomicptr Error;
	__xrt_ws_output* OutputHead;
	__xrt_ws_output* OutputTail;
	xatomic64 OutputBytes;
	xatomic32 ReadPaused;
	xatomic32 DrivePosted;
	uint64 CloseTimer;
	xnetpost DriveCommand;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xspinlock AsyncLock;
		__xrt_ws_async* AsyncSendHead;
		__xrt_ws_async* AsyncSendTail;
		__xrt_ws_async* AsyncWaitHead;
		__xrt_ws_async* AsyncWaitTail;
		xatomic64 AsyncBytes;
		xatomic32 AsyncCount;
		xnetpost AsyncCommand;
		bool AsyncPosted;
		bool AsyncDriving;
		bool AsyncNotified;
	#endif
	bool FrameActive;
	bool MessageOpen;
	bool Reading;
	bool CloseSent;
	bool CloseReceived;
	bool RemoteInitiated;
	bool ProtocolFailed;
	bool ProtocolPeerActivity;
	bool TransportClosing;
	bool Backpressured;
	bool DrainPending;
	bool ErrorEmitted;
	bool CloseEmitted;
};

#endif


#endif
