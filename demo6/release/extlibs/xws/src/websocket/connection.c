#include "../internal/xrt_websocket.h"



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

#define __XRT_WS_MASK_CHUNK 1024u



/* 创建 Connection 域错误，并保留完整底层原因链。 */
xerror* __xrtWsConnErrorCreate(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.connection";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	return xrtErrorBuild(&Desc);
}



/* 保存第一个不可恢复错误，并同步设置当前线程错误。 */
static const xerror* __xrtWsConnRemember(
	xwsconn* pConnection,
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtWsConnErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	ptr pExpected = NULL;
	const xerror* pStored;

	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	if ( (pConnection != NULL) && (pError != NULL) ) {
		if ( !xrtAtomicPtrCompareExchange(
			&pConnection->Error,
			&pExpected,
			pError,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			xrtErrorFree(pError);
		}
		pStored = (const xerror*)xrtAtomicPtrLoad(
			&pConnection->Error,
			XMEMORY_ACQUIRE
		);
	} else {
		pStored = pError;
	}
	if ( pStored != NULL ) {
		xrtSetError(pStored);
	}
	if ( pConnection == NULL ) {
		xrtErrorFree(pError);
	}
	return pStored;
}



/* 设置一次调用错误，不污染 Connection 保存的不可恢复错误。 */
const xerror* __xrtWsConnReject(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	return __xrtWsConnRemember(
		NULL,
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 第一次错误发布与第一个保存错误保持一一对应。 */
static void __xrtWsConnEmitError(xwsconn* pConnection)
{
	const xerror* pError;

	if ( (pConnection == NULL) || pConnection->ErrorEmitted ) {
		return;
	}
	pError = (const xerror*)xrtAtomicPtrLoad(
		&pConnection->Error,
		XMEMORY_ACQUIRE
	);
	if ( pError == NULL ) {
		return;
	}
	pConnection->ErrorEmitted = true;
	if ( pConnection->Events.Error != NULL ) {
		pConnection->Events.Error(
			pConnection,
			pError,
			pConnection->Data
		);
	}
}



/* 返回指定角色最大控制帧在线路上占用的字节数。 */
static size_t __xrtWsConnControlSlot(xwsrole Role)
{
	return 2u + XWS_CLOSE_PAYLOAD_MAX +
		(Role == XWS_ROLE_CLIENT ? XWS_MASK_SIZE : 0u);
}



/* 验证基础边界，并把协商参数映射到本地压缩方向。 */
static bool __xrtWsConnConfigPrepare(xwsconnconfig* pConfig)
{
	size_t iControlSlot;
	size_t iControlMinimum;
	size_t iDataMinimum;

	if ( (pConfig == NULL) ||
		((pConfig->Role != XWS_ROLE_CLIENT) &&
		 (pConfig->Role != XWS_ROLE_SERVER)) ||
		!xrtMemRangeValid(
			pConfig->Protocol.Data,
			pConfig->Protocol.Size
		) ||
		(pConfig->Protocol.Size >
		 (SIZE_MAX - sizeof(xwsconn) - 1u)) ||
		(pConfig->MessageLimit == 0) ||
		(pConfig->FrameLimit == 0) ||
		(pConfig->FrameLimit > XWS_FRAME_PAYLOAD_MAX) ||
		(pConfig->FrameLimit > (uint64)SIZE_MAX) ) {
		return false;
	}
	iControlSlot = __xrtWsConnControlSlot(pConfig->Role);
	iControlMinimum = iControlSlot * 3u;
	if ( (pConfig->ControlReserve < iControlMinimum) ||
		(pConfig->ControlReserve > pConfig->SendLimit) ) {
		return false;
	}
	iDataMinimum = 2u + (
		pConfig->Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u
	);
	if ( (pConfig->SendLimit -
		 pConfig->ControlReserve) <
		iDataMinimum ) {
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		if ( (pConfig->AsyncBytesLimit == 0) ||
			(pConfig->AsyncCountLimit == 0) ||
			(pConfig->AsyncBatch == 0) ) {
			return false;
		}
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConfig->DeflateEnabled ) {
			xwsdeflatedirection Receive;
			xwsdeflatedirection Send;
			size_t iSize = 0;

			if ( !xrtWsDeflateResponseWrite(
				&pConfig->Deflate,
				NULL,
				0,
				&iSize
			) || !xrtWsDeflateDirection(
				&pConfig->Deflate,
				pConfig->Role,
				false,
				&Receive
			) || !xrtWsDeflateDirection(
				&pConfig->Deflate,
				pConfig->Role,
				true,
				&Send
			) || !xrtWsInflaterConfigApply(
				&pConfig->Inflater,
				&Receive
			) || !xrtWsDeflaterConfigApply(
				&pConfig->Deflater,
				&Send
			) ) {
				return false;
			}
		}
	#endif
	return true;
}



/* 对外部适配层执行无分配且不修改输入的 Connection 配置预检。 */
XRT_API bool xrtWsConnConfigValid(
	const xwsconnconfig* pConfig
)
{
	xwsconnconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return __xrtWsConnConfigPrepare(&Config);
}



/* 验证 Connection 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsConnRangeValid(const xwsconn* pConnection)
{
	return xrtMemRangeValid(
		pConnection,
		sizeof(*pConnection)
	);
}



/* 验证公开入口收到的 Connection，并设置稳定的结构化错误。 */
static bool __xrtWsConnCheck(
	const xwsconn* pConnection,
	cstr sOperation
)
{
	if ( __xrtWsConnRangeValid(pConnection) ) {
		return true;
	}
	(void)__xrtWsConnReject(
		XERR_ARGUMENT,
		XWS_CONN_ERROR_ARGUMENT,
		sOperation,
		"WebSocket connection range is invalid",
		NULL
	);
	return false;
}



/* 已关闭对象可从任意线程查询，活动操作必须留在传输 Worker。 */
bool __xrtWsConnWorker(
	xwsconn* pConnection,
	cstr sOperation
)
{
	if ( !__xrtWsConnCheck(pConnection, sOperation) ) {
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			sOperation,
			"WebSocket connection operation requires its network worker",
			NULL
		);
		return false;
	}
	return true;
}



/* 可裁剪地推进 Connection Future 适配层。 */
static void __xrtWsConnNotifyFutures(xwsconn* pConnection)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		__xrtWsConnFutureNotify(pConnection);
	#else
		(void)pConnection;
	#endif
}



/* 短暂增加底层传输引用，避免并发 Abort 和终态回调发生悬空访问。 */
static ptr __xrtWsConnTransportRef(
	const xwsconn* pConnection
)
{
	xwsconn* pMutable = (xwsconn*)pConnection;
	ptr pTransport;

	if ( pMutable == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pMutable->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pMutable->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pMutable->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				pTransport = xrtTlsStreamRef(
					(xtlsstream*)pTransport
				);
			} else
		#endif
		{
			pTransport = xrtNetStreamRef(
				(xnetstream*)pTransport
			);
		}
	}
	xrtSpinUnlock(&pMutable->TransportLock);
	return pTransport;
}



/* 释放由传输快照取得的临时引用。 */
static void __xrtWsConnTransportRelease(
	const xwsconn* pConnection,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	(void)pConnection;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
			return;
		}
	#endif
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 返回当前传输已经受理但尚未排空的字节数。 */
static size_t __xrtWsConnTransportPending(
	const xwsconn* pConnection
)
{
	ptr pTransport = __xrtWsConnTransportRef(pConnection);
	size_t iPending = 0;

	if ( pTransport != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pConnection->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				iPending = xrtTlsStreamPending(
					(xtlsstream*)pTransport
				);
			} else
		#endif
		{
			iPending = xrtNetStreamPending(
				(xnetstream*)pTransport
			);
		}
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iPending;
}



/* 释放 TLS 尚未受理的全部精确帧余量。 */
static void __xrtWsConnOutputClear(xwsconn* pConnection)
{
	__xrt_ws_output* pOutput = pConnection->OutputHead;

	pConnection->OutputHead = NULL;
	pConnection->OutputTail = NULL;
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	while ( pOutput != NULL ) {
		__xrt_ws_output* pNext = pOutput->Next;

		xrtFree(pOutput);
		pOutput = pNext;
	}
}



/* 最后一个 Connection 引用释放所有非传输资源。 */
static void __xrtWsConnFree(xwsconn* pConnection)
{
	xerror* pError;

	if ( pConnection == NULL ) {
		return;
	}
	__xrtWsConnOutputClear(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xrtWsInflaterDestroy(pConnection->Inflater);
		xrtWsDeflaterDestroy(pConnection->Deflater);
	#endif
	pError = (xerror*)xrtAtomicPtrExchange(
		&pConnection->Error,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtErrorFree(pError);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xrtSpinUnit(&pConnection->AsyncLock);
	#endif
	xrtSpinUnit(&pConnection->TransportLock);
	xrtFree(pConnection);
}



/* TCP 零复制发送完成后释放包含帧数据的单一分配。 */
static void __xrtWsConnOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	xrtFree(pContext);
}



/* 合并 TLS 余量和传输队列，溢出时返回饱和值。 */
XRT_API size_t xrtWsConnPending(const xwsconn* pConnection)
{
	uint64 iOutput;
	size_t iTransport;

	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-pending"
	) ) {
		return 0;
	}
	iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	iTransport = __xrtWsConnTransportPending(pConnection);
	if ( iOutput > (uint64)(SIZE_MAX - iTransport) ) {
		return SIZE_MAX;
	}
	return iTransport + (size_t)iOutput;
}



/* 把 WebSocket 线路字节换算为当前传输实际占用的发送预算。 */
static bool __xrtWsConnTransportSize(
	const xwsconn* pConnection,
	size_t iSize,
	size_t* pBudget
)
{
	size_t iRecords;

	if ( iSize == 0 ) {
		*pBudget = 0;
		return true;
	}
	if ( pConnection->SendOverhead == 0 ) {
		*pBudget = iSize;
		return true;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		iRecords = (iSize / XTLS_RECORD_PLAINTEXT_MAX) +
			((iSize % XTLS_RECORD_PLAINTEXT_MAX) != 0 ? 1u : 0u);
		if ( iRecords > ((SIZE_MAX - iSize) /
			pConnection->SendOverhead) ) {
			return false;
		}
		*pBudget = iSize +
			(iRecords * pConnection->SendOverhead);
		return true;
	#else
		(void)iRecords;
		return false;
	#endif
}



/* 返回发送类别必须留给更高优先级协议帧的传输预算。 */
static size_t __xrtWsConnReserve(
	const xwsconn* pConnection,
	__xrt_ws_send_class Class
)
{
	switch ( Class ) {
		case __XRT_WS_SEND_DATA:
			return pConnection->Config.ControlReserve;
		case __XRT_WS_SEND_CONTROL:
			return pConnection->ControlSlot * 2u;
		case __XRT_WS_SEND_AUTO_PONG:
			return pConnection->ControlSlot;
		case __XRT_WS_SEND_CLOSE:
		default:
			return 0;
	}
}



/* 在一个总容量中扣除当前类别不能占用的协议预留。 */
static size_t __xrtWsConnClassCapacity(
	size_t iCapacity,
	size_t iReserve
)
{
	return iCapacity > iReserve ?
		iCapacity - iReserve : 0;
}



/*
	返回一帧永久能够占用的发送容量。
	TCP 的 WriteLimit 可能小于 WebSocket SendLimit，必须在提交前共同取小值；
	TLS 短写余量由 Connection 自身队列承接，因此只受 SendLimit 约束。
*/
static size_t __xrtWsConnCapacity(
	const xwsconn* pConnection,
	__xrt_ws_send_class Class
)
{
	size_t iReserve = __xrtWsConnReserve(pConnection, Class);
	size_t iCapacity = __xrtWsConnClassCapacity(
		pConnection->Config.SendLimit,
		iReserve
	);
	ptr pTransport;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return iCapacity;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsConnClassCapacity(
			xrtNetStreamWriteLimit((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iCapacity ) {
			iCapacity = iTcp;
		}
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iCapacity;
}



/* 前置声明供公开普通数据可写查询复用同一容量口径。 */
static size_t __xrtWsConnAvailable(
	const xwsconn* pConnection,
	__xrt_ws_send_class Class
);



/* 计算控制预留之后普通数据仍可使用的发送预算。 */
XRT_API size_t xrtWsConnWritable(const xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-writable"
	) || (xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return 0;
	}
	return __xrtWsConnAvailable(
		pConnection,
		__XRT_WS_SEND_DATA
	);
}



/* 按发送类别扣除更高优先级协议帧的固定预留。 */
static size_t __xrtWsConnAvailable(
	const xwsconn* pConnection,
	__xrt_ws_send_class Class
)
{
	size_t iPending = xrtWsConnPending(pConnection);
	size_t iReserve = __xrtWsConnReserve(pConnection, Class);
	size_t iAvailable;
	ptr pTransport;

	if ( iPending >= pConnection->Config.SendLimit ) {
		return 0;
	}
	iAvailable = __xrtWsConnClassCapacity(
		pConnection->Config.SendLimit - iPending,
		iReserve
	);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return iAvailable;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsConnClassCapacity(
			xrtNetStreamWritable((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iAvailable ) {
			iAvailable = iTcp;
		}
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iAvailable;
}



/* 普通发送第一次遇到硬预算时发布一次背压边沿。 */
void __xrtWsConnBackpressure(xwsconn* pConnection)
{
	if ( pConnection->Backpressured ) {
		return;
	}
	pConnection->Backpressured = true;
	if ( pConnection->Events.Backpressure != NULL ) {
		pConnection->Events.Backpressure(
			pConnection,
			xrtWsConnPending(pConnection),
			pConnection->Data
		);
	}
}



/* 发送预算恢复后发布一次可写边沿。 */
static void __xrtWsConnWritableEvent(xwsconn* pConnection)
{
	if ( !pConnection->Backpressured ||
		(xrtWsConnWritable(pConnection) == 0) ) {
		return;
	}
	pConnection->Backpressured = false;
	if ( pConnection->Events.Writable != NULL ) {
		pConnection->Events.Writable(
			pConnection,
			xrtWsConnPending(pConnection),
			pConnection->Data
		);
	}
}



/* 创建一个头部与负载同分配的完整线路帧。 */
static __xrt_ws_output* __xrtWsConnFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	bool bCompressed
)
{
	xwsframe Frame;
	xwsframeconfig Config;
	__xrt_ws_output* pOutput;
	size_t iHead = 0;
	size_t iTotal;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	Frame.PayloadSize = Payload.Size;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bCompressed ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		Frame.Flags |= XWS_FRAME_MASKED;
		if ( !xrtSecureRandom(Frame.Mask, sizeof(Frame.Mask)) ) {
			(void)__xrtWsConnReject(
				XERR_IO,
				XWS_CONN_ERROR_RANDOM,
				"write-websocket-frame",
				"WebSocket client could not generate a frame mask",
				xrtGetError()
			);
			return NULL;
		}
	}
	xrtWsFrameConfigInit(&Config);
	if ( bCompressed ) {
		Config.AllowedRsv = XWS_FRAME_RSV1;
	}
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		NULL,
		0,
		&iHead
	) || (Payload.Size > (SIZE_MAX - iHead)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_FRAME,
			"write-websocket-frame",
			"WebSocket frame size is not representable",
			xrtGetError()
		);
		return NULL;
	}
	iTotal = iHead + Payload.Size;
	if ( iTotal > (SIZE_MAX - sizeof(*pOutput)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-websocket-frame",
			"WebSocket frame allocation size overflowed",
			NULL
		);
		return NULL;
	}
	pOutput = (struct __xrt_ws_output*)xrtMalloc(
		sizeof(*pOutput) + iTotal
	);
	if ( pOutput == NULL ) {
		(void)__xrtWsConnReject(
			XERR_MEMORY,
			XWS_CONN_ERROR_MEMORY,
			"write-websocket-frame",
			"WebSocket frame allocation failed",
			NULL
		);
		return NULL;
	}
	memset(pOutput, 0, sizeof(*pOutput));
	pOutput->Size = iTotal;
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		pOutput->Data,
		iHead,
		&iHead
	) ) {
		xrtFree(pOutput);
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_FRAME,
			"write-websocket-frame",
			"WebSocket frame header construction failed",
			xrtGetError()
		);
		return NULL;
	}
	if ( Payload.Size != 0 ) {
		memcpy(
			pOutput->Data + iHead,
			Payload.Data,
			Payload.Size
		);
		if ( (Frame.Flags & XWS_FRAME_MASKED) != 0 ) {
			(void)xrtWsMask(
				pOutput->Data + iHead,
				Payload.Size,
				Frame.Mask,
				0
			);
		}
	}
	return pOutput;
}



/* 不分配内存地计算完整线路帧大小，用于先执行硬预算检查。 */
static bool __xrtWsConnFrameSize(
	const xwsconn* pConnection,
	size_t iPayload,
	size_t* pSize
)
{
	size_t iHead = 2u;

	if ( iPayload > UINT16_MAX ) {
		iHead += 8u;
	} else if ( iPayload > XWS_CLOSE_PAYLOAD_MAX ) {
		iHead += 2u;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		iHead += XWS_MASK_SIZE;
	}
	if ( iPayload > (SIZE_MAX - iHead) ) {
		return false;
	}
	*pSize = iHead + iPayload;
	return true;
}



/* 在分配前统一检查帧上限、永久容量和瞬时预算。 */
xnetresult __xrtWsConnFrameBudget(
	xwsconn* pConnection,
	size_t iPayload,
	__xrt_ws_send_class Class,
	size_t* pWireSize
)
{
	size_t iWireSize;
	size_t iBudget;
	bool bControl = Class != __XRT_WS_SEND_DATA;

	if ( (pConnection == NULL) || (pWireSize == NULL) ||
		(Class < __XRT_WS_SEND_DATA) ||
		(Class > __XRT_WS_SEND_CLOSE) ) {
		__xwsErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( (bControl && (iPayload > XWS_CLOSE_PAYLOAD_MAX)) ||
		(!bControl &&
		 ((uint64)iPayload > pConnection->Config.FrameLimit)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			bControl ?
				"WebSocket control payload exceeds 125 bytes" :
				"WebSocket data payload exceeds its frame limit",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnFrameSize(
		pConnection,
		iPayload,
		&iWireSize
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnTransportSize(
		pConnection,
		iWireSize,
		&iBudget
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnCapacity(
		pConnection,
		Class
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame exceeds its permanent send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnAvailable(
		pConnection,
		Class
	) ) {
		if ( Class == __XRT_WS_SEND_DATA ) {
			__xrtWsConnBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	*pWireSize = iWireSize;
	return XNET_RESULT_OK;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 把 TLS 尚未受理的帧尾追加到精确余量队列。 */
static void __xrtWsConnOutputAppend(
	xwsconn* pConnection,
	__xrt_ws_output* pOutput
)
{
	pOutput->Next = NULL;
	if ( pConnection->OutputTail != NULL ) {
		pConnection->OutputTail->Next = pOutput;
	} else {
		pConnection->OutputHead = pOutput;
	}
	pConnection->OutputTail = pOutput;
	(void)xrtAtomic64FetchAdd(
		&pConnection->OutputBytes,
		(uint64)pOutput->Pending,
		XMEMORY_RELEASE
	);
}
#endif



/* 区分可重试的同步发送失败与已经破坏传输的会话故障。 */
void __xrtWsConnSendFailure(
	xwsconn* pConnection,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( bFatal ) {
		(void)__xrtWsConnRemember(
			pConnection,
			Kind,
			XWS_CONN_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	} else {
		(void)__xrtWsConnReject(
			Kind,
			XWS_CONN_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	}
}



/* 向 TCP 或 TLS 提交一整个已计入预算的帧。 */
static xnetresult __xrtWsConnSubmit(
	xwsconn* pConnection,
	__xrt_ws_output* pOutput,
	__xrt_ws_send_class Class
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	size_t iBudget;
	xnetresult Result;

	if ( pTransport == NULL ) {
		xrtFree(pOutput);
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtWsConnTransportSize(
		pConnection,
		pOutput->Size,
		&iBudget
	) ) {
		xrtFree(pOutput);
		__xrtWsConnSendFailure(
			pConnection,
			false,
			XERR_RANGE,
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnAvailable(
		pConnection,
		Class
	) ) {
		xrtFree(pOutput);
		if ( Class == __XRT_WS_SEND_DATA ) {
			__xrtWsConnBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			size_t iWritten = 0;
			xtlsresult TlsResult;

			if ( pConnection->OutputHead != NULL ) {
				pOutput->Pending = iBudget;
				__xrtWsConnOutputAppend(
					pConnection,
					pOutput
				);
				pConnection->DrainPending = true;
				return XNET_RESULT_OK;
			}
			TlsResult = xrtTlsStreamSend(
				(xtlsstream*)pTransport,
				pOutput->Data,
				pOutput->Size,
				&iWritten
			);
			pOutput->Offset = iWritten;
			if ( (TlsResult == XTLS_ERROR) ||
				(TlsResult == XTLS_CLOSED) ) {
				const xerror* pCause =
					xrtTlsStreamError(
						(xtlsstream*)pTransport
					);

				xrtFree(pOutput);
				if ( pCause == NULL ) {
					pCause = xrtGetError();
				}
				__xrtWsConnSendFailure(
					pConnection,
					xrtTlsStreamState(
						(xtlsstream*)pTransport
					) != XTLS_STREAM_OPEN,
					TlsResult == XTLS_CLOSED ?
						XERR_CLOSED : XERR_IO,
					"TLS rejected a WebSocket frame",
					pCause
				);
				return TlsResult == XTLS_CLOSED ?
					XNET_RESULT_CLOSED :
					XNET_RESULT_ERROR;
			}
			if ( pOutput->Offset < pOutput->Size ) {
				if ( !__xrtWsConnTransportSize(
					pConnection,
					pOutput->Size - pOutput->Offset,
					&pOutput->Pending
				) ) {
					xrtFree(pOutput);
					__xrtWsConnSendFailure(
						pConnection,
						true,
						XERR_INTERNAL,
						"TLS short-write budget became invalid",
						NULL
					);
					return XNET_RESULT_ERROR;
				}
				__xrtWsConnOutputAppend(
					pConnection,
					pOutput
				);
			} else {
				xrtFree(pOutput);
			}
			pConnection->DrainPending = true;
			return XNET_RESULT_OK;
		}
	#endif
	Result = xrtNetStreamSendRef(
		(xnetstream*)pTransport,
		pOutput->Data,
		pOutput->Size,
		__xrtWsConnOutputRelease,
		pOutput
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pOutput);
		if ( (Result == XNET_RESULT_AGAIN) &&
			(Class == __XRT_WS_SEND_DATA) ) {
			__xrtWsConnBackpressure(pConnection);
		} else if ( Result == XNET_RESULT_ERROR ) {
			const xerror* pCause = xrtNetStreamError(
				(xnetstream*)pTransport
			);

			if ( pCause == NULL ) {
				pCause = xrtGetError();
			}
			__xrtWsConnSendFailure(
				pConnection,
				xrtNetStreamState(
					(xnetstream*)pTransport
				) != XNET_STREAM_OPEN,
				pCause != NULL ?
					xrtErrorKind(pCause) : XERR_IO,
				"TCP rejected a WebSocket frame",
				pCause
			);
		}
		return Result;
	}
	pConnection->DrainPending = true;
	return XNET_RESULT_OK;
}



/* 验证并发送一个数据或控制帧。 */
xnetresult __xrtWsConnSendFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xrt_ws_send_class Class,
	bool bCompressed
)
{
	__xrt_ws_output* pOutput;
	size_t iWireSize;
	xnetresult Budget;

	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-frame",
			"WebSocket payload range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	Budget = __xrtWsConnFrameBudget(
		pConnection,
		Payload.Size,
		Class,
		&iWireSize
	);
	if ( Budget != XNET_RESULT_OK ) {
		return Budget;
	}
	pOutput = __xrtWsConnFrame(
		pConnection,
		Opcode,
		Payload,
		bFinal,
		bCompressed
	);
	if ( pOutput == NULL ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtWsConnSubmit(
		pConnection,
		pOutput,
		Class
	);
}



/* 取消关闭计时器；终态回调仍负责释放其 Connection 引用。 */
static void __xrtWsConnCancelCloseTimer(xwsconn* pConnection)
{
	xnetengine* pEngine;

	if ( pConnection->CloseTimer == 0 ) {
		return;
	}
	pEngine = xrtNetWorkerEngine(pConnection->Worker);
	if ( !xrtNetEngineTimerCancelCurrent(
		pEngine,
		pConnection->CloseTimer
	) && !xrtNetEngineTimerCancel(
		pEngine,
		pConnection->CloseTimer
	) ) {
		xrtClearError();
	}
}



/* 排空条件满足后开始唯一的底层传输关闭。 */
static void __xrtWsConnCloseTransportStart(xwsconn* pConnection)
{
	ptr pTransport;
	bool bAccepted;

	if ( pConnection->TransportClosing ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	pConnection->TransportClosing = true;
	__xrtWsConnCancelCloseTimer(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			bAccepted = xrtTlsStreamClose(
				(xtlsstream*)pTransport
			);
		} else
	#endif
	{
		bAccepted = xrtNetStreamClose(
			(xnetstream*)pTransport
		);
	}
	if ( !bAccepted ) {
		xrtClearError();
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 远端 Close 或协议失败后的新输入到达后，排空输出并关闭底层传输。 */
static void __xrtWsConnCloseTransport(xwsconn* pConnection)
{
	if ( (!pConnection->CloseReceived &&
		 !(pConnection->ProtocolFailed &&
		   pConnection->ProtocolPeerActivity)) ||
		(pConnection->OutputHead != NULL) ) {
		return;
	}
	__xrtWsConnCloseTransportStart(pConnection);
}



/* 关闭计时器只在远端没有回应时把握手变为明确超时。 */
static void __xrtWsConnCloseTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pWorker;
	if ( pConnection->CloseTimer == Id ) {
		pConnection->CloseTimer = 0;
		if ( (Result == XNET_RESULT_OK) &&
			!pConnection->CloseReceived &&
			(xrtWsConnState(pConnection) !=
			 XWS_CONN_CLOSED) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_TIMEOUT,
				XWS_CONN_ERROR_TIMEOUT,
				"close-websocket",
				"WebSocket close handshake timed out",
				NULL
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
		}
	}
	xrtWsConnDestroy(pConnection);
}



/* 首个本地 Close 建立独立握手超时。 */
static bool __xrtWsConnStartCloseTimer(xwsconn* pConnection)
{
	if ( (pConnection->Config.CloseTimeout == 0) ||
		(pConnection->CloseTimer != 0) ||
		pConnection->CloseReceived ) {
		return true;
	}
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return false;
	}
	pConnection->CloseTimer = xrtNetEngineAfter(
		xrtNetWorkerEngine(pConnection->Worker),
		xrtNetWorkerIndex(pConnection->Worker),
		pConnection->Config.CloseTimeout,
		__xrtWsConnCloseTimer,
		pConnection
	);
	if ( pConnection->CloseTimer == 0 ) {
		xrtWsConnDestroy(pConnection);
		(void)__xrtWsConnRemember(
			pConnection,
			XERR_AGAIN,
			XWS_CONN_ERROR_TIMEOUT,
			"close-websocket",
			"WebSocket close timer could not be scheduled",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 在同步终态重入期间保护会话，并提交唯一的本地 Close。 */
static xnetresult __xrtWsConnClosePayload(
	xwsconn* pConnection,
	xbytesview Payload,
	uint16 iCode,
	bool bRemote
)
{
	uint32 iState;
	xnetresult Result;

	if ( xrtWsConnRef(pConnection) == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	xrtSpinLock(&pConnection->TransportLock);
	if ( pConnection->CloseSent ||
		((iState = xrtAtomic32Load(
			&pConnection->State,
			XMEMORY_ACQUIRE
		 )) != XWS_CONN_OPEN) ) {
		xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsConnDestroy(pConnection);
		return XNET_RESULT_CLOSED;
	}
	/*
		先发布唯一 Close 意图。底层发送可同步触发终态回调，
		终态快照必须在该重入窗口内看到完整的本地关闭信息。
	*/
	pConnection->CloseSent = true;
	pConnection->LocalCode = iCode;
	pConnection->RemoteInitiated = bRemote;
	xrtSpinUnlock(&pConnection->TransportLock);
	Result = __xrtWsConnSendFrame(
		pConnection,
		XWS_OPCODE_CLOSE,
		Payload,
		true,
		__XRT_WS_SEND_CLOSE,
		false
	);
	if ( Result != XNET_RESULT_OK ) {
		/* 未受理的 Close 不占用唯一发送槽，调用方可以重试。 */
		xrtSpinLock(&pConnection->TransportLock);
		if ( xrtWsConnState(pConnection) != XWS_CONN_CLOSED ) {
			pConnection->CloseSent = false;
			pConnection->LocalCode = 0;
			pConnection->RemoteInitiated = false;
		}
		xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsConnDestroy(pConnection);
		return Result;
	}
	/* 同步终态可能已经写入 CLOSED，只允许从 OPEN 单向推进。 */
	iState = XWS_CONN_OPEN;
	(void)xrtAtomic32CompareExchange(
		&pConnection->State,
		&iState,
		XWS_CONN_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	if ( (iState != XWS_CONN_CLOSED) &&
		xrtWsConnPaused(pConnection) ) {
		(void)xrtWsConnResume(pConnection);
	}
	if ( (iState != XWS_CONN_CLOSED) &&
		!__xrtWsConnStartCloseTimer(pConnection) ) {
		__xrtWsConnEmitError(pConnection);
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( iState != XWS_CONN_CLOSED ) {
		__xrtWsConnCloseTransport(pConnection);
	}
	xrtWsConnDestroy(pConnection);
	return XNET_RESULT_OK;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS 可写边沿继续提交之前发生短写的精确帧余量。 */
static bool __xrtWsConnOutputDrive(xwsconn* pConnection)
{
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	while ( (pStream != NULL) &&
		(pConnection->OutputHead != NULL) ) {
		__xrt_ws_output* pOutput =
			pConnection->OutputHead;
		size_t iRemaining =
			pOutput->Size - pOutput->Offset;
		size_t iPending = pOutput->Pending;
		size_t iWritten = 0;
		xtlsresult Result = xrtTlsStreamSend(
			pStream,
			pOutput->Data + pOutput->Offset,
			iRemaining,
			&iWritten
		);

		if ( iWritten != 0 ) {
			pOutput->Offset += iWritten;
			if ( !__xrtWsConnTransportSize(
				pConnection,
				pOutput->Size - pOutput->Offset,
				&pOutput->Pending
			) || (pOutput->Pending > iPending) ) {
				(void)__xrtWsConnRemember(
					pConnection,
					XERR_INTERNAL,
					XWS_CONN_ERROR_SEND,
					"drain-websocket-output",
					"TLS short-write accounting became invalid",
					NULL
				);
				__xrtWsConnEmitError(pConnection);
				(void)xrtWsConnAbort(pConnection);
				return false;
			}
			(void)xrtAtomic64FetchSub(
				&pConnection->OutputBytes,
				(uint64)(iPending - pOutput->Pending),
				XMEMORY_RELEASE
			);
		}
		if ( pOutput->Offset == pOutput->Size ) {
			pConnection->OutputHead = pOutput->Next;
			if ( pConnection->OutputHead == NULL ) {
				pConnection->OutputTail = NULL;
			}
			xrtFree(pOutput);
		}
		if ( (Result == XTLS_ERROR) ||
			(Result == XTLS_CLOSED) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				Result == XTLS_CLOSED ?
					XERR_CLOSED : XERR_IO,
				XWS_CONN_ERROR_SEND,
				"drain-websocket-output",
				"TLS could not continue a WebSocket frame",
				xrtTlsStreamError(pStream)
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
			return false;
		}
		if ( (Result == XTLS_AGAIN) ||
			(iWritten == 0) ) {
			break;
		}
	}
	if ( pConnection->OutputHead == NULL ) {
		__xrtWsConnWritableEvent(pConnection);
		__xrtWsConnCloseTransport(pConnection);
	}
	return true;
}
#endif



/* 返回当前 TCP 或 TLS 明文接收链。 */
static const xnetbuf* __xrtWsConnBuffer(xwsconn* pConnection)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return xrtTlsStreamBuffer(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return xrtNetStreamBuffer((xnetstream*)pTransport);
}



/* 精确消费 TCP 或 TLS 明文，并保持各自恢复读取的内部契约。 */
static bool __xrtWsConnConsume(
	xwsconn* pConnection,
	size_t iSize
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return xrtTlsStreamConsume(
				(xtlsstream*)pTransport,
				iSize
			);
		}
	#endif
	return xrtNetStreamConsume(
		(xnetstream*)pTransport,
		iSize
	) == iSize;
}



/* 丢弃协议失败时已经进入明文缓冲的不可恢复输入。 */
static bool __xrtWsConnDiscardInput(xwsconn* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsConnBuffer(pConnection);
	size_t iSize = pBuffer != NULL ?
		xrtNetBufSize(pBuffer) : 0;

	return (iSize == 0) ||
		__xrtWsConnConsume(pConnection, iSize);
}



/* 校验一段扩展解码后的语义负载并发布消息数据。 */
static bool __xrtWsConnPayloadSemantic(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	xwsmessageerrorinfo Error;

	if ( !xrtWsMessagePayload(
		&pConnection->Message,
		Payload,
		&Error
	) ) {
		pConnection->FailureCloseCode =
			Error.CloseCode != 0 ?
			Error.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"read-websocket-message",
			"WebSocket message payload is invalid",
			xrtGetError()
		);
		return false;
	}
	if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) != 0 ) {
		if ( Payload.Size >
			(sizeof(pConnection->Control) -
			 pConnection->ControlSize) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_CONN_ERROR_MESSAGE,
				"read-websocket-control",
				"WebSocket control payload overflowed",
				NULL
			);
			return false;
		}
		if ( Payload.Size != 0 ) {
			memcpy(
				pConnection->Control +
					pConnection->ControlSize,
				Payload.Data,
				Payload.Size
			);
		}
		pConnection->ControlSize += Payload.Size;
	} else if ( pConnection->MessageOpen &&
		(pConnection->Events.MessageData != NULL) ) {
		pConnection->Events.MessageData(
			pConnection,
			Payload,
			pConnection->Data
		);
	}
	return true;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 把 Inflater 的同步输出接入消息校验与应用事件。 */
static bool __xrtWsConnInflateOutput(
	xbytesview Data,
	ptr pData
)
{
	return __xrtWsConnPayloadSemantic(
		(xwsconn*)pData,
		Data
	);
}



/* 把压缩层错误固定映射为消息过大或非法扩展数据。 */
static bool __xrtWsConnInflateFailure(
	xwsconn* pConnection,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	if ( pConnection->FailureCloseCode != 0 ) {
		return false;
	}
	pConnection->FailureCloseCode =
		(pCause != NULL) &&
		(xrtErrorDomain(pCause) != NULL) &&
		(strcmp(
			xrtErrorDomain(pCause),
			"xrt.websocket.deflate"
		 ) == 0) &&
		(xrtErrorCode(pCause) ==
		 XWS_DEFLATE_ERROR_LIMIT) ?
			XWS_CLOSE_TOO_BIG :
			XWS_CLOSE_INVALID_DATA;
	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ?
			xrtErrorKind(pCause) : XERR_PROTOCOL,
		XWS_CONN_ERROR_MESSAGE,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}
#endif



/* 解码一段线路负载；未协商压缩时保持零复制直通。 */
static bool __xrtWsConnPayloadWire(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_CONTROL) == 0) ) {
			if ( !xrtWsInflaterWrite(
				pConnection->Inflater,
				Payload,
				__xrtWsConnInflateOutput,
				pConnection
			) ) {
				return __xrtWsConnInflateFailure(
					pConnection,
					"inflate-websocket-message",
					"WebSocket compressed payload is invalid"
				);
			}
			return true;
		}
	#endif
	return __xrtWsConnPayloadSemantic(
		pConnection,
		Payload
	);
}



/* 把本地控制帧提交失败映射为 1011，而不是归咎于对端协议。 */
static bool __xrtWsConnControlFailure(
	xwsconn* pConnection,
	xnetresult Result,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	pConnection->FailureCloseCode = XWS_CLOSE_INTERNAL;
	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ? xrtErrorKind(pCause) :
			(Result == XNET_RESULT_AGAIN ? XERR_AGAIN : XERR_IO),
		XWS_CONN_ERROR_SEND,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 处理已经完整校验的 Ping、Pong 或 Close 控制帧。 */
static bool __xrtWsConnControl(xwsconn* pConnection)
{
	xbytesview Payload = {
		pConnection->Control,
		pConnection->ControlSize
	};

	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_PING ) {
		if ( pConnection->Config.AutoPong &&
			(xrtWsConnState(pConnection) == XWS_CONN_OPEN) ) {
			xnetresult Result = __xrtWsConnSendFrame(
				pConnection,
				XWS_OPCODE_PONG,
				Payload,
				true,
				__XRT_WS_SEND_AUTO_PONG,
				false
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsConnControlFailure(
					pConnection,
					Result,
					"reply-websocket-ping",
					"WebSocket automatic Pong could not be submitted"
				);
			}
		}
		if ( pConnection->Events.Ping != NULL ) {
			pConnection->Events.Ping(
				pConnection,
				Payload,
				pConnection->Data
			);
		}
		return true;
	}
	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_PONG ) {
		if ( pConnection->Events.Pong != NULL ) {
			pConnection->Events.Pong(
				pConnection,
				Payload,
				pConnection->Data
			);
		}
		return true;
	}
	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_CLOSE ) {
		xwsclose Close;

		if ( !xrtWsCloseParse(Payload, &Close) ) {
			pConnection->FailureCloseCode =
				(xrtErrorCode(xrtGetError()) ==
				 XWS_CLOSE_ERROR_UTF8) ?
				XWS_CLOSE_INVALID_DATA :
				XWS_CLOSE_PROTOCOL;
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_CONN_ERROR_MESSAGE,
				"read-websocket-close",
				"WebSocket Close payload is invalid",
				xrtGetError()
			);
			return false;
		}
		xrtSpinLock(&pConnection->TransportLock);
		pConnection->CloseReceived = true;
		pConnection->RemoteCode = Close.Code;
		pConnection->RemoteReasonSize =
			(uint16)Close.Reason.Size;
		if ( Close.Reason.Size != 0 ) {
			memcpy(
				pConnection->RemoteReason,
				Close.Reason.Data,
				Close.Reason.Size
			);
		}
		pConnection->RemoteReason[
			pConnection->RemoteReasonSize
		] = '\0';
		xrtSpinUnlock(&pConnection->TransportLock);
		if ( !pConnection->CloseSent ) {
			xnetresult Result = __xrtWsConnClosePayload(
				pConnection,
				Payload,
				Close.Code,
				true
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsConnControlFailure(
					pConnection,
					Result,
					"reply-websocket-close",
					"WebSocket Close reply could not be submitted"
				);
			}
		} else {
			__xrtWsConnCloseTransport(pConnection);
		}
		return true;
	}
	return false;
}



/* 完成当前帧，并在协议校验成功后发布消息 End 或控制事件。 */
static bool __xrtWsConnFrameEnd(xwsconn* pConnection)
{
	xwsmessageerrorinfo Error;
	bool bControl =
		(pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) != 0;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			!bControl &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_END) != 0) &&
			!xrtWsInflaterEnd(
				pConnection->Inflater,
				__xrtWsConnInflateOutput,
				pConnection
			) ) {
			return __xrtWsConnInflateFailure(
				pConnection,
				"finish-websocket-inflate",
				"WebSocket compressed message could not finish"
			);
		}
	#endif
	if ( !xrtWsMessageFrameEnd(
		&pConnection->Message,
		&Error
	) ) {
		pConnection->FailureCloseCode =
			Error.CloseCode != 0 ?
			Error.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"finish-websocket-frame",
			"WebSocket message frame is incomplete or invalid",
			xrtGetError()
		);
		return false;
	}
	if ( bControl ) {
		if ( !__xrtWsConnControl(pConnection) ) {
			return false;
		}
	} else if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_END) != 0 ) {
		if ( pConnection->MessageOpen &&
			(pConnection->Events.MessageEnd != NULL) ) {
			pConnection->Events.MessageEnd(
				pConnection,
				pConnection->Data
			);
		}
		pConnection->MessageOpen = false;
	}
	pConnection->FrameActive = false;
	pConnection->FrameRemaining = 0;
	pConnection->FrameOffset = 0;
	pConnection->ControlSize = 0;
	return true;
}



/* 解析一个完整帧头并建立消息层语义状态。 */
static int __xrtWsConnFrameBegin(xwsconn* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsConnBuffer(pConnection);
	xwsframeconfig Config;
	xwsframeerrorinfo FrameError;
	xwsmessageerrorinfo MessageError;
	uint8 Header[XWS_FRAME_HEAD_MAX];
	size_t iAvailable;
	size_t iHeader;
	xwsframestatus Status;

	if ( pBuffer == NULL ) {
		return -1;
	}
	iAvailable = xrtNetBufSize(pBuffer);
	if ( iAvailable == 0 ) {
		return 0;
	}
	iHeader = iAvailable < sizeof(Header) ?
		iAvailable : sizeof(Header);
	if ( xrtNetBufPeek(
		pBuffer,
		0,
		Header,
		iHeader
	) != iHeader ) {
		return -1;
	}
	xrtWsFrameConfigInit(&Config);
	Config.MaxPayload = pConnection->Config.FrameLimit;
	Config.Mask = pConnection->Config.Role ==
		XWS_ROLE_SERVER ?
		XWS_MASK_REQUIRED : XWS_MASK_FORBIDDEN;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConnection->Config.DeflateEnabled ) {
			Config.AllowedRsv = XWS_FRAME_RSV1;
		}
	#endif
	Status = xrtWsFrameParse(
		(xbytesview) { Header, iHeader },
		&pConnection->Frame,
		&Config,
		&FrameError
	);
	if ( Status == XWS_FRAME_MORE ) {
		return 0;
	}
	if ( Status == XWS_FRAME_ERROR ) {
		pConnection->FailureCloseCode =
			(FrameError.Code == XWS_FRAME_ERROR_LENGTH) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE) ?
				XWS_CLOSE_TOO_BIG : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_FRAME,
			"read-websocket-frame",
			"WebSocket frame header is invalid",
			xrtGetError()
		);
		return -1;
	}
	if ( !__xrtWsConnConsume(
		pConnection,
		pConnection->Frame.HeadSize
	) ) {
		(void)__xrtWsConnRemember(
			pConnection,
			XERR_INTERNAL,
			XWS_CONN_ERROR_TRANSPORT,
			"consume-websocket-frame",
			"WebSocket transport did not consume its frame header",
			xrtGetError()
		);
		return -2;
	}
	if ( !xrtWsMessageFrameBegin(
		&pConnection->Message,
		&pConnection->Frame,
		&pConnection->MessageInfo,
		&MessageError
	) ) {
		pConnection->FailureCloseCode =
			MessageError.CloseCode != 0 ?
			MessageError.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"begin-websocket-message",
			"WebSocket frame violates message sequencing",
			xrtGetError()
		);
		return -1;
	}
	pConnection->FrameActive = true;
	pConnection->FrameRemaining =
		pConnection->Frame.PayloadSize;
	pConnection->FrameOffset = 0;
	pConnection->ControlSize = 0;
	if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) == 0 ) {
		if ( (pConnection->MessageInfo.Flags &
			 XWS_MESSAGE_BEGIN) != 0 ) {
			#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
				if ( pConnection->Config.DeflateEnabled &&
					!xrtWsInflaterBegin(
						pConnection->Inflater,
						(pConnection->MessageInfo.Flags &
						 XWS_MESSAGE_EXTENDED) != 0
					) ) {
					(void)__xrtWsConnInflateFailure(
						pConnection,
						"begin-websocket-inflate",
						"WebSocket compressed message could not begin"
					);
					return -1;
				}
			#endif
			pConnection->MessageOpen = true;
			if ( pConnection->Events.MessageBegin != NULL ) {
				#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
					if ( (pConnection->MessageInfo.Rsv &
						  XWS_FRAME_RSV1) != 0 ) {
						pConnection->MessageInfo.Flags |=
							XWS_MESSAGE_COMPRESSED;
					}
				#endif
				pConnection->Events.MessageBegin(
					pConnection,
					&pConnection->MessageInfo,
					pConnection->Data
				);
			}
		}
	}
	return 1;
}



/* 失败后优先发送标准 Close；控制预算也不可用时立即中止。 */
static void __xrtWsConnProtocolFail(
	xwsconn* pConnection,
	uint16 iCloseCode
)
{
	uint8 Payload[2];
	xbytesview View;

	__xrtWsConnEmitError(pConnection);
	if ( pConnection->CloseSent ||
		(xrtWsConnState(pConnection) == XWS_CONN_CLOSED) ) {
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	pConnection->ProtocolFailed = true;
	Payload[0] = (uint8)(iCloseCode >> 8u);
	Payload[1] = (uint8)iCloseCode;
	View.Data = Payload;
	View.Size = sizeof(Payload);
	if ( __xrtWsConnClosePayload(
		pConnection,
		View,
		iCloseCode,
		false
	) != XNET_RESULT_OK ) {
		(void)xrtWsConnAbort(pConnection);
	} else if ( !__xrtWsConnDiscardInput(pConnection) ) {
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 增量消费任意网络分块，不为消息或帧建立固定连接缓冲。 */
static void __xrtWsConnRead(xwsconn* pConnection)
{
	uint8 Scratch[__XRT_WS_MASK_CHUNK];

	if ( pConnection->Reading ||
		pConnection->ProtocolFailed ||
		xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		return;
	}
	/*
		消息与控制帧回调可以同步关闭底层传输并释放其 Connection 引用。
		读取状态机必须独立持有活动引用，直到不再访问任何会话字段。
	*/
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return;
	}
	pConnection->Reading = true;
	while ( (xrtWsConnState(pConnection) !=
		 XWS_CONN_CLOSED) &&
		!xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		const xnetbuf* pBuffer;
		xnetspan Span;
		size_t iChunk;

		if ( !pConnection->FrameActive ) {
			int iBegin = __xrtWsConnFrameBegin(pConnection);

			if ( iBegin == 0 ) {
				break;
			}
			if ( iBegin < 0 ) {
				if ( iBegin == -2 ) {
					__xrtWsConnEmitError(pConnection);
					(void)xrtWsConnAbort(pConnection);
				} else {
					__xrtWsConnProtocolFail(
						pConnection,
						pConnection->FailureCloseCode != 0 ?
							pConnection->FailureCloseCode :
							XWS_CLOSE_PROTOCOL
					);
				}
				break;
			}
			if ( xrtAtomic32Load(
				&pConnection->ReadPaused,
				XMEMORY_ACQUIRE
			) ) {
				break;
			}
		}
		if ( pConnection->FrameRemaining == 0 ) {
			if ( !__xrtWsConnFrameEnd(pConnection) ) {
				__xrtWsConnProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
			continue;
		}
		pBuffer = __xrtWsConnBuffer(pConnection);
		if ( (pBuffer == NULL) ||
			!xrtNetBufFront(pBuffer, &Span) ) {
			break;
		}
		iChunk = Span.Size;
		if ( (uint64)iChunk >
			pConnection->FrameRemaining ) {
			iChunk = (size_t)
				pConnection->FrameRemaining;
		}
		if ( (pConnection->Frame.Flags &
			 XWS_FRAME_MASKED) != 0 ) {
			if ( iChunk > sizeof(Scratch) ) {
				iChunk = sizeof(Scratch);
			}
			memcpy(Scratch, Span.Data, iChunk);
			if ( !xrtWsMask(
				Scratch,
				iChunk,
				pConnection->Frame.Mask,
				pConnection->FrameOffset
			) || !__xrtWsConnPayloadWire(
				pConnection,
				(xbytesview) { Scratch, iChunk }
			) ) {
				__xrtWsConnProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
		} else if ( !__xrtWsConnPayloadWire(
			pConnection,
			(xbytesview) { Span.Data, iChunk }
		) ) {
			__xrtWsConnProtocolFail(
				pConnection,
				pConnection->FailureCloseCode != 0 ?
					pConnection->FailureCloseCode :
					XWS_CLOSE_PROTOCOL
			);
			break;
		}
		if ( !__xrtWsConnConsume(pConnection, iChunk) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_INTERNAL,
				XWS_CONN_ERROR_TRANSPORT,
				"consume-websocket-input",
				"WebSocket transport did not consume its payload",
				xrtGetError()
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
			break;
		}
		pConnection->FrameRemaining -= iChunk;
		pConnection->FrameOffset += iChunk;
	}
	pConnection->Reading = false;
	xrtWsConnDestroy(pConnection);
}



/* 在所属 Worker 上处理早到数据，并按应用暂停状态恢复 TCP 读取。 */
static void __xrtWsConnDrive(
	xnetworker* pWorker,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pConnection->DrivePosted,
		0,
		XMEMORY_RELEASE
	);
	if ( !xrtAtomic32Load(
		&pConnection->ReadPaused,
		XMEMORY_ACQUIRE
	) ) {
		__xrtWsConnRead(pConnection);
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			xrtWsConnDestroy(pConnection);
			return;
		}
	#endif
	if ( (xrtWsConnState(pConnection) !=
		 XWS_CONN_CLOSED) &&
		!xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
			&pConnection->Transport,
			XMEMORY_ACQUIRE
		);

		if ( (pStream != NULL) &&
			!xrtNetStreamResume(pStream) ) {
			xrtClearError();
		}
	}
	xrtWsConnDestroy(pConnection);
}



/* 合并任意线程的恢复请求，并为嵌入命令持有一份 Connection 引用。 */
static void __xrtWsConnDriveSchedule(xwsconn* pConnection)
{
	uint32 iExpected = 0;

	if ( !xrtAtomic32CompareExchange(
		&pConnection->DrivePosted,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( xrtWsConnRef(pConnection) == NULL ) {
		xrtAtomic32Store(
			&pConnection->DrivePosted,
			0,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->DriveCommand,
		__xrtWsConnDrive,
		pConnection
	) ) {
		xrtAtomic32Store(
			&pConnection->DrivePosted,
			0,
			XMEMORY_RELEASE
		);
		xrtWsConnDestroy(pConnection);
	}
}



/* 暂停后续应用消息分块；TCP 立即停止新接收，TLS 由未消费明文施加背压。 */
XRT_API void xrtWsConnPause(xwsconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"pause-websocket-read"
	) || (xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return;
	}
	xrtAtomic32Store(
		&pConnection->ReadPaused,
		1,
		XMEMORY_RELEASE
	);
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		xrtAtomic32Store(
			&pConnection->ReadPaused,
			0,
			XMEMORY_RELEASE
		);
		__xrtWsConnDriveSchedule(pConnection);
		return;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		xrtNetStreamPause((xnetstream*)pTransport);
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
}



/* 恢复读取，并让所属 Worker 继续消费已缓冲的 TCP 或 TLS 明文。 */
XRT_API bool xrtWsConnResume(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"resume-websocket-read",
			"WebSocket connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"resume-websocket-read"
	) ) {
		return false;
	}
	if ( xrtWsConnState(pConnection) == XWS_CONN_CLOSED ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
			"resume-websocket-read",
			"WebSocket connection is closed",
			NULL
		);
		return false;
	}
	xrtAtomic32Store(
		&pConnection->ReadPaused,
		0,
		XMEMORY_RELEASE
	);
	__xrtWsConnDriveSchedule(pConnection);
	return true;
}



/* 返回应用读取暂停状态的并发快照。 */
XRT_API bool xrtWsConnPaused(const xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-paused"
	) ) {
		return false;
	}
	return xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) != 0;
}



/* 对端 TCP 半关闭且没有 Close 帧时终止为非完整 WebSocket 关闭。 */
static void __xrtWsConnTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	if ( !pConnection->CloseReceived ) {
		pConnection->TransportClosing = true;
		if ( !xrtNetStreamClose(pStream) ) {
			xrtClearError();
			(void)xrtNetStreamAbort(pStream);
		}
	}
}



/* TCP Read 直接驱动统一消息状态机。 */
static void __xrtWsConnTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsConnDiscardInput(pConnection) ) {
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsConnCloseTransport(pConnection);
		return;
	}
	if ( xrtWsConnPaused(pConnection) ) {
		xrtNetStreamPause(pStream);
		return;
	}
	__xrtWsConnRead(pConnection);
}



/* TCP 高水位和 Connection 自身预算共同折叠为一个背压状态。 */
static void __xrtWsConnTcpHigh(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsConnBackpressure((xwsconn*)pData);
}



/* TCP 回落到低水位后重新计算真正可写预算。 */
static void __xrtWsConnTcpLow(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsConnWritableEvent((xwsconn*)pData);
	__xrtWsConnNotifyFutures((xwsconn*)pData);
}



/* 两级队列排空后发布唯一 Drain 边沿。 */
static void __xrtWsConnDrain(xwsconn* pConnection)
{
	if ( !pConnection->DrainPending ||
		(xrtWsConnPending(pConnection) != 0) ) {
		return;
	}
	pConnection->DrainPending = false;
	__xrtWsConnWritableEvent(pConnection);
	if ( pConnection->Events.Drain != NULL ) {
		pConnection->Events.Drain(
			pConnection,
			pConnection->Data
		);
	}
}



/* TCP 排空回调完成 WebSocket Drain 边沿。 */
static void __xrtWsConnTcpDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtWsConnDrain((xwsconn*)pData);
	__xrtWsConnNotifyFutures((xwsconn*)pData);
}



/* 复制当前 Close 终态供同步回调和后续查询共用。 */
static void __xrtWsConnCloseSnapshot(
	const xwsconn* pConnection,
	xwsconnclose* pClose
)
{
	xwsconn* pMutable = (xwsconn*)pConnection;
	xwsconnclose Close;

	memset(&Close, 0, sizeof(Close));
	xrtSpinLock(&pMutable->TransportLock);
	if ( pConnection->CloseSent ) {
		Close.Flags |= XWS_CONN_CLOSE_SENT;
	}
	if ( pConnection->CloseReceived ) {
		Close.Flags |= XWS_CONN_CLOSE_RECEIVED;
	}
	if ( pConnection->CloseSent &&
		pConnection->CloseReceived &&
		((xnetresult)xrtAtomic32Load(
			&pConnection->TransportResult,
			XMEMORY_ACQUIRE
		 ) == XNET_RESULT_OK) ) {
		Close.Flags |= XWS_CONN_CLOSE_CLEAN;
	}
	if ( pConnection->RemoteInitiated ) {
		Close.Flags |= XWS_CONN_CLOSE_REMOTE;
	}
	Close.Transport = (xnetresult)xrtAtomic32Load(
		&pConnection->TransportResult,
		XMEMORY_ACQUIRE
	);
	Close.LocalCode = pConnection->LocalCode;
	Close.RemoteCode = pConnection->RemoteCode;
	Close.Reason.Data = pConnection->RemoteReason;
	Close.Reason.Size = pConnection->RemoteReasonSize;
	xrtSpinUnlock(&pMutable->TransportLock);
	memcpy(pClose, &Close, sizeof(Close));
}



/* 验证 Connection 及其协商协议副本占用的完整连续存储。 */
bool __xrtWsConnStorageRange(
	const xwsconn* pConnection,
	size_t* pSize
)
{
	size_t iSize;

	if ( !__xrtWsConnRangeValid(pConnection) ||
		(pConnection->Protocol.Size >
		 (SIZE_MAX - sizeof(*pConnection) - 1u)) ) {
		return false;
	}
	iSize = sizeof(*pConnection) +
		pConnection->Protocol.Size + 1u;
	if ( !xrtMemRangeValid(pConnection, iSize) ) {
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 统一处理 TCP/TLS 传输终态并释放 Connection 的传输引用。 */
static void __xrtWsConnTransportClose(
	xwsconn* pConnection,
	ptr pTransport,
	xnetresult Result,
	const xerror* pError
)
{
	xwsconnclose Close;
	ptr pOwned;

	xrtAtomic32Store(
		&pConnection->TransportResult,
		(uint32)Result,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XWS_CONN_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtWsConnCancelCloseTimer(pConnection);
	if ( (Result != XNET_RESULT_OK) &&
		(pError != NULL) &&
		(xrtWsConnError(pConnection) == NULL) ) {
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(pError),
			XWS_CONN_ERROR_TRANSPORT,
			"close-websocket-transport",
			"WebSocket transport closed with an error",
			pError
		);
		__xrtWsConnEmitError(pConnection);
	}
	__xrtWsConnOutputClear(pConnection);
	if ( !pConnection->CloseEmitted ) {
		pConnection->CloseEmitted = true;
		__xrtWsConnCloseSnapshot(pConnection, &Close);
		if ( pConnection->Events.Close != NULL ) {
			pConnection->Events.Close(
				pConnection,
				&Close,
				pConnection->Data
			);
		}
	}
	__xrtWsConnNotifyFutures(pConnection);
	xrtSpinLock(&pConnection->TransportLock);
	pOwned = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtSpinUnlock(&pConnection->TransportLock);
	if ( pOwned != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pConnection->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				xrtTlsStreamDestroy(
					(xtlsstream*)pOwned
				);
			} else
		#endif
		{
			xrtNetStreamDestroy(
				(xnetstream*)pOwned
			);
		}
	}
	(void)pTransport;
	xrtWsConnDestroy(pConnection);
}



/* TCP Close 转入统一 WebSocket 终态。 */
static void __xrtWsConnTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsConnTransportClose(
		(xwsconn*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TCP 事件表。 */
static const xnetstreamevents* __xrtWsConnTcpEvents(void)
{
	static const xnetstreamevents Events = {
		NULL,
		__xrtWsConnTcpRead,
		__xrtWsConnTcpEnd,
		__xrtWsConnTcpHigh,
		__xrtWsConnTcpLow,
		__xrtWsConnTcpDrain,
		__xrtWsConnTcpClose
	};

	return &Events;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS Read 驱动同一消息状态机。 */
static void __xrtWsConnTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsConnDiscardInput(pConnection) ) {
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsConnCloseTransport(pConnection);
		return;
	}
	__xrtWsConnRead(pConnection);
}



/* TLS 明文写空间恢复后先排空 Connection 精确余量。 */
static void __xrtWsConnTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	if ( __xrtWsConnOutputDrive(pConnection) ) {
		__xrtWsConnWritableEvent(pConnection);
		__xrtWsConnNotifyFutures(pConnection);
	}
}



/* TLS 两级发送队列排空后发布 Connection Drain。 */
static void __xrtWsConnTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	if ( __xrtWsConnOutputDrive(pConnection) ) {
		__xrtWsConnDrain(pConnection);
		__xrtWsConnNotifyFutures(pConnection);
	}
}



/* TLS 对端完成 close_notify 时继续认证关闭。 */
static void __xrtWsConnTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	if ( !pConnection->TransportClosing ) {
		pConnection->TransportClosing = true;
		if ( !xrtTlsStreamClose(pStream) ) {
			xrtClearError();
			(void)xrtTlsStreamAbort(pStream);
		}
	}
}



/* TLS Close 转入统一 WebSocket 终态。 */
static void __xrtWsConnTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsConnTransportClose(
		(xwsconn*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TLS 事件表。 */
static const xtlsstreamevents* __xrtWsConnTlsEvents(void)
{
	static const xtlsstreamevents Events = {
		NULL,
		__xrtWsConnTlsRead,
		__xrtWsConnTlsEnd,
		__xrtWsConnTlsWritable,
		__xrtWsConnTlsDrain,
		__xrtWsConnTlsClose
		#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
			, NULL
		#endif
	};

	return &Events;
}
#endif



/* 创建共享 Connection 状态，但不提前改变传输事件所有权。 */
static xwsconn* __xrtWsConnCreate(
	xnetworker* pWorker,
	__xrt_ws_transport TransportKind,
	ptr pTransport,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xwsconnconfig Config;
	xwsconnevents Events;
	xwsmessageconfig MessageConfig;
	xwsconn* pConnection;
	size_t iAllocation;

	xrtWsConnConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			(void)__xrtWsConnRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_CONN_ERROR_ARGUMENT,
				"attach-websocket",
				"WebSocket connection config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
			(void)__xrtWsConnRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_CONN_ERROR_ARGUMENT,
				"attach-websocket",
				"WebSocket connection event range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	if ( !__xrtWsConnConfigPrepare(&Config) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_VALUE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket connection configuration is invalid",
			NULL
		);
		return NULL;
	}
	iAllocation = sizeof(*pConnection) +
		Config.Protocol.Size + 1u;
	pConnection = (xwsconn*)xrtCalloc(1, iAllocation);
	if ( pConnection == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_MEMORY,
			XWS_CONN_ERROR_MEMORY,
			"attach-websocket",
			"WebSocket connection allocation failed",
			NULL
		);
		return NULL;
	}
	pConnection->References = 2;
	xrtAtomic32Init(&pConnection->State, XWS_CONN_OPEN);
	xrtAtomic32Init(
		&pConnection->TransportResult,
		XNET_RESULT_OK
	);
	xrtAtomicPtrInit(&pConnection->Transport, pTransport);
	xrtAtomicPtrInit(&pConnection->Error, NULL);
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	xrtAtomic32Init(&pConnection->ReadPaused, 0);
	xrtAtomic32Init(&pConnection->DrivePosted, 0);
	if ( !xrtNetPostInit(&pConnection->DriveCommand) ) {
		xrtFree(pConnection);
		return NULL;
	}
	xrtSpinInit(&pConnection->TransportLock);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xrtAtomic64Init(&pConnection->AsyncBytes, 0);
		xrtAtomic32Init(&pConnection->AsyncCount, 0);
		if ( !xrtNetPostInit(&pConnection->AsyncCommand) ) {
			xrtFree(pConnection);
			return NULL;
		}
		xrtSpinInit(&pConnection->AsyncLock);
	#endif
	pConnection->Worker = pWorker;
	pConnection->TransportKind = TransportKind;
	pConnection->Config = Config;
	if ( Config.Protocol.Size != 0 ) {
		char* sProtocol = (char*)(pConnection + 1);

		memcpy(
			sProtocol,
			Config.Protocol.Data,
			Config.Protocol.Size
		);
		sProtocol[Config.Protocol.Size] = '\0';
		pConnection->Protocol.Data = sProtocol;
		pConnection->Protocol.Size =
			Config.Protocol.Size;
		pConnection->Config.Protocol =
			pConnection->Protocol;
	} else {
		memset(
			&pConnection->Config.Protocol,
			0,
			sizeof(pConnection->Config.Protocol)
		);
	}
	pConnection->Events = Events;
	pConnection->Data = pData;
	xrtWsMessageConfigInit(&MessageConfig);
	MessageConfig.MaxSize = Config.MessageLimit;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( Config.DeflateEnabled ) {
			MessageConfig.FirstRsv = XWS_FRAME_RSV1;
		}
	#endif
	if ( !xrtWsMessageInit(
		&pConnection->Message,
		&MessageConfig
	) ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
			xrtSpinUnit(&pConnection->AsyncLock);
		#endif
		xrtSpinUnit(&pConnection->TransportLock);
		xrtFree(pConnection);
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( Config.DeflateEnabled ) {
			pConnection->Inflater = xrtWsInflaterCreate(
				&Config.Inflater
			);
			pConnection->Deflater = xrtWsDeflaterCreate(
				&Config.Deflater
			);
			if ( (pConnection->Inflater == NULL) ||
				(pConnection->Deflater == NULL) ) {
				xrtWsInflaterDestroy(
					pConnection->Inflater
				);
				xrtWsDeflaterDestroy(
					pConnection->Deflater
				);
				xrtSpinUnit(
					&pConnection->TransportLock
				);
				#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
					xrtSpinUnit(
						&pConnection->AsyncLock
					);
				#endif
				xrtFree(pConnection);
				(void)__xrtWsConnRemember(
					NULL,
					XERR_MEMORY,
					XWS_CONN_ERROR_MEMORY,
					"attach-websocket-deflate",
					"WebSocket compression state allocation failed",
					xrtGetError()
				);
				return NULL;
			}
		}
	#endif
	return pConnection;
}



/* 初始化稳定的已建立会话默认值。 */
XRT_API void xrtWsConnConfigInit(xwsconnconfig* pConfig)
{
	xwsconnconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"config-init-websocket",
			"WebSocket connection configuration range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Role = XWS_ROLE_SERVER;
	Config.MessageLimit =
		XWS_CONN_MESSAGE_LIMIT_DEFAULT;
	Config.FrameLimit =
		XWS_CONN_FRAME_LIMIT_DEFAULT;
	Config.SendLimit = XWS_CONN_SEND_LIMIT_DEFAULT;
	Config.ControlReserve =
		XWS_CONN_CONTROL_RESERVE_DEFAULT;
	Config.CloseTimeout =
		XWS_CONN_CLOSE_TIMEOUT_DEFAULT;
	Config.AutoPong = true;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		Config.AsyncBytesLimit =
			XWS_CONN_ASYNC_BYTES_DEFAULT;
		Config.AsyncCountLimit =
			XWS_CONN_ASYNC_COUNT_DEFAULT;
		Config.AsyncBatch =
			XWS_CONN_ASYNC_BATCH_DEFAULT;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		xrtWsInflaterConfigInit(&Config.Inflater);
		xrtWsDeflaterConfigInit(&Config.Deflater);
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* Attach 失败时只销毁 Connection 草稿，不接管或关闭输入传输。 */
static void __xrtWsConnAttachDiscard(xwsconn* pConnection)
{
	pConnection->References = 1;
	xrtAtomicPtrStore(
		&pConnection->Transport,
		NULL,
		XMEMORY_RELEASE
	);
	xrtWsConnDestroy(pConnection);
}



/*
	把控制槽换算到实际传输成本，并验证普通数据至少能容纳一个空帧。
	TLS 记录开销在握手完成后固定，Connection 后续可以无查询地精确计账。
*/
static bool __xrtWsConnAttachBudget(
	xwsconn* pConnection,
	ptr pTransport
)
{
	size_t iControl = __xrtWsConnControlSlot(
		pConnection->Config.Role
	);
	size_t iDataFrame = 2u + (
		pConnection->Config.Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u
	);
	size_t iDataBudget;
	size_t iMinimum;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			size_t iOne;
			size_t iFull;

			if ( !xrtTlsStreamSendBound(
				(xtlsstream*)pTransport,
				1u,
				&iOne
			) || !xrtTlsStreamSendBound(
				(xtlsstream*)pTransport,
				XTLS_RECORD_PLAINTEXT_MAX,
				&iFull
			) || (iOne <= 1u) ||
				(iFull <= XTLS_RECORD_PLAINTEXT_MAX) ||
				((iOne - 1u) !=
				 (iFull - XTLS_RECORD_PLAINTEXT_MAX)) ) {
				return false;
			}
			pConnection->SendOverhead = iOne - 1u;
		}
	#endif
	if ( !__xrtWsConnTransportSize(
		pConnection,
		iControl,
		&pConnection->ControlSlot
	) || !__xrtWsConnTransportSize(
		pConnection,
		iDataFrame,
		&iDataBudget
	) || (pConnection->ControlSlot > (SIZE_MAX / 3u)) ) {
		return false;
	}
	iMinimum = pConnection->ControlSlot * 3u;
	if ( (pConnection->Config.ControlReserve < iMinimum) ||
		(iDataBudget > (pConnection->Config.SendLimit -
		 pConnection->Config.ControlReserve)) ) {
		return false;
	}
	if ( pConnection->TransportKind == __XRT_WS_TRANSPORT_TCP ) {
		size_t iWriteLimit = xrtNetStreamWriteLimit(
			(xnetstream*)pTransport
		);

		if ( (pConnection->Config.ControlReserve > iWriteLimit) ||
			(iDataBudget > (iWriteLimit -
			 pConnection->Config.ControlReserve)) ) {
			return false;
		}
	}
	return true;
}



/* 接管开放 TCP Stream，并延迟处理已经缓冲的早到帧。 */
XRT_API xwsconn* xrtWsConnAttach(
	xnetstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xnetworker* pWorker;
	xwsconn* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"attach-websocket",
			"WebSocket TCP Stream is null",
			NULL
		);
		return NULL;
	}
	pWorker = xrtNetStreamWorker(pStream);
	if ( (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"attach-websocket",
			"WebSocket TCP Stream is not open on its worker",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsConnCreate(
		pWorker,
		__XRT_WS_TRANSPORT_TCP,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnAttachBudget(pConnection, pStream) ) {
		__xrtWsConnAttachDiscard(pConnection);
		(void)__xrtWsConnRemember(
			NULL,
			XERR_RANGE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket send budget is incompatible with its TCP Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtNetStreamSetEvents(
		pStream,
		__xrtWsConnTcpEvents(),
		pConnection
	) ) {
		__xrtWsConnAttachDiscard(pConnection);
		return NULL;
	}
	__xrtWsConnDriveSchedule(pConnection);
	return pConnection;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 接管开放 TLS Stream，并延迟处理已经解密的早到明文。 */
XRT_API xwsconn* xrtWsConnAttachTls(
	xtlsstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xnetstream* pTransport;
	xnetworker* pWorker;
	xwsconn* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"attach-websocket-tls",
			"WebSocket TLS Stream is null",
			NULL
		);
		return NULL;
	}
	pTransport = xrtTlsStreamTransport(pStream);
	pWorker = xrtNetStreamWorker(pTransport);
	if ( (pTransport == NULL) || (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(xrtTlsStreamState(pStream) !=
		 XTLS_STREAM_OPEN) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"attach-websocket-tls",
			"WebSocket TLS Stream is not open on its worker",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsConnCreate(
		pWorker,
		__XRT_WS_TRANSPORT_TLS,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnAttachBudget(pConnection, pStream) ) {
		__xrtWsConnAttachDiscard(pConnection);
		(void)__xrtWsConnRemember(
			NULL,
			XERR_RANGE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket-tls",
			"WebSocket send budget is incompatible with its TLS Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtTlsStreamSetEvents(
		pStream,
		__xrtWsConnTlsEvents(),
		pConnection
	) ) {
		__xrtWsConnAttachDiscard(pConnection);
		return NULL;
	}
	__xrtWsConnDriveSchedule(pConnection);
	return pConnection;
}
#endif



/* 增加 Connection 引用。 */
XRT_API xwsconn* xrtWsConnRef(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket"
	) || (xrtRefRetain(&pConnection->References) < 0) ) {
		return NULL;
	}
	return pConnection;
}



/* 释放 Connection 引用。 */
XRT_API void xrtWsConnDestroy(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"release-websocket"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pConnection->References) == 0 ) {
		__xrtWsConnFree(pConnection);
	}
}



/* 返回并发可读状态。 */
XRT_API xwsconnstate xrtWsConnState(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_CONN_CLOSED;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-state"
	) ) {
		return XWS_CONN_CLOSED;
	}
	return (xwsconnstate)xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回固定本端角色。 */
XRT_API xwsrole xrtWsConnRole(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_ROLE_SERVER;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-role"
	) ) {
		return XWS_ROLE_SERVER;
	}
	return pConnection->Config.Role;
}



/* 返回拥有型已协商子协议的借用视图。 */
XRT_API xstrview xrtWsConnProtocol(
	const xwsconn* pConnection
)
{
	xstrview Empty = { 0 };

	if ( pConnection == NULL ) {
		return Empty;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-protocol"
	) ) {
		return Empty;
	}
	return pConnection->Protocol;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 复制固定的 permessage-deflate 协商结果。 */
XRT_API bool xrtWsConnDeflate(
	const xwsconn* pConnection,
	xwsdeflate* pDeflate
)
{
	xwsdeflate Deflate;
	size_t iConnectionSize;

	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-deflate"
	) ) {
		return false;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnectionSize
	) || !xrtMemRangeValid(pDeflate, sizeof(Deflate)) ||
		xrtMemRangesOverlap(
			pDeflate,
			sizeof(Deflate),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"query-websocket-deflate",
			"WebSocket connection or disjoint Deflate output is invalid",
			NULL
		);
		return false;
	}
	if ( !pConnection->Config.DeflateEnabled ) {
		return false;
	}
	Deflate = pConnection->Config.Deflate;
	memcpy(pDeflate, &Deflate, sizeof(Deflate));
	return true;
}
#endif



/* 返回借用 Worker。 */
XRT_API xnetworker* xrtWsConnWorker(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-worker"
	) ) {
		return NULL;
	}
	return pConnection->Worker;
}



/* TCP 会话返回借用 Stream。 */
XRT_API xnetstream* xrtWsConnTcp(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TCP) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"query-websocket-tcp",
			"borrowed WebSocket TCP Stream requires its network worker",
			NULL
		);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}



/* 从任意线程取得 TCP Stream 强引用。 */
XRT_API xnetstream* xrtWsConnTcpRef(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TCP) ) {
		return NULL;
	}
	return (xnetstream*)__xrtWsConnTransportRef(
		pConnection
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS 会话返回借用 Stream。 */
XRT_API xtlsstream* xrtWsConnTls(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TLS) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"query-websocket-tls",
			"borrowed WebSocket TLS Stream requires its network worker",
			NULL
		);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}



/* 从任意线程取得 TLS Stream 强引用。 */
XRT_API xtlsstream* xrtWsConnTlsRef(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TLS) ) {
		return NULL;
	}
	return (xtlsstream*)__xrtWsConnTransportRef(
		pConnection
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 单条压缩消息只按实际输出增长，不在连接对象中保留固定缓冲。 */
typedef struct __xrt_ws_compressed {
	bytes Data;
	size_t Size;
	size_t Capacity;
	size_t Limit;
	xnetbuf* Buffer;
} __xrt_ws_compressed;



/* 在当前线路预算内收集 Deflater 的同步输出。 */
static bool __xrtWsConnDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xrt_ws_compressed* pOutput =
		(__xrt_ws_compressed*)pData;
	size_t iRequired;
	size_t iCapacity;
	bytes pBytes;

	if ( (pOutput->Size > pOutput->Limit) ||
		(Data.Size >
		 (pOutput->Limit - pOutput->Size)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"compress-websocket-message",
			"compressed WebSocket message exceeds its permanent send capacity",
			NULL
		);
		return false;
	}
	iRequired = pOutput->Size + Data.Size;
	if ( pOutput->Buffer != NULL ) {
		if ( !xrtNetBufAppend(
			pOutput->Buffer,
			Data.Data,
			Data.Size
		) ) {
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"compress-websocket-message",
				"compressed WebSocket buffer allocation failed",
				xrtGetError()
			);
			return false;
		}
		pOutput->Size = iRequired;
		return true;
	}
	if ( iRequired > pOutput->Capacity ) {
		iCapacity = pOutput->Capacity != 0 ?
			pOutput->Capacity : 256u;
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity <=
				(pOutput->Limit / 2u) ?
					(iCapacity * 2u) :
					pOutput->Limit;

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		pBytes = (bytes)xrtRealloc(
			pOutput->Data,
			iCapacity
		);
		if ( pBytes == NULL ) {
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"compress-websocket-message",
				"compressed WebSocket output allocation failed",
				xrtGetError()
			);
			return false;
		}
		pOutput->Data = pBytes;
		pOutput->Capacity = iCapacity;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pOutput->Data + pOutput->Size,
			Data.Data,
			Data.Size
		);
	}
	pOutput->Size = iRequired;
	return true;
}



/* 为服务端明文压缩结果前置帧头并把 Worker 缓冲链直接转交 TCP。 */
static xnetresult __xrtWsConnDeflateBufferSubmit(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xnetbuf* pBuffer,
	size_t iPayload,
	bool bFirst,
	bool bFinal
)
{
	xwsframe Frame;
	xwsframeconfig Config;
	xnetstream* pStream;
	uint8 pHead[XWS_FRAME_HEAD_MAX];
	size_t iHead = 0;
	size_t iWireSize;
	xnetresult Result;

	Result = __xrtWsConnFrameBudget(
		pConnection,
		iPayload,
		__XRT_WS_SEND_DATA,
		&iWireSize
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bFirst ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	Frame.PayloadSize = iPayload;
	xrtWsFrameConfigInit(&Config);
	Config.AllowedRsv = XWS_FRAME_RSV1;
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		pHead,
		sizeof(pHead),
		&iHead
	) || !xrtNetBufPrepend(pBuffer, pHead, iHead) ) {
		(void)__xrtWsConnReject(
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_INTERNAL,
			XWS_CONN_ERROR_FRAME,
			"compress-websocket-message",
			"compressed WebSocket frame construction failed",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pStream = xrtWsConnTcp(pConnection);
	if ( pStream == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	Result = xrtNetStreamSendBuffer(pStream, pBuffer);
	if ( Result == XNET_RESULT_OK ) {
		pConnection->DrainPending = true;
		return Result;
	}
	if ( Result == XNET_RESULT_AGAIN ) {
		__xrtWsConnBackpressure(pConnection);
	} else if ( Result == XNET_RESULT_ERROR ) {
		const xerror* pCause = xrtNetStreamError(pStream);

		if ( pCause == NULL ) {
			pCause = xrtGetError();
		}
		__xrtWsConnSendFailure(
			pConnection,
			xrtNetStreamState(pStream) != XNET_STREAM_OPEN,
			pCause != NULL ? xrtErrorKind(pCause) : XERR_IO,
			"TCP rejected a compressed WebSocket buffer",
			pCause
		);
	}
	return Result;
}



/* 回滚未发送消息的发送上下文，同时保留原始调用错误。 */
static void __xrtWsConnDeflateRollback(
	xwsconn* pConnection
)
{
	xerror* pError = xrtTakeError();

	if ( !xrtWsDeflaterAbort(pConnection->Deflater) ) {
		xrtClearError();
	}
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/* 流式压缩帧把最终线路节点和最大负载放在同一分配中。 */
typedef struct __xrt_ws_deflate_frame {
	__xrt_ws_output* Output;
	size_t Size;
	size_t Limit;
	xnetbuf* Buffer;
} __xrt_ws_deflate_frame;



/* 把 Deflater 的同步输出直接追加到已经预留的最终帧负载区。 */
static bool __xrtWsConnDeflateFrameOutput(
	xbytesview Data,
	ptr pData
)
{
	__xrt_ws_deflate_frame* pFrame =
		(__xrt_ws_deflate_frame*)pData;

	if ( (pFrame->Size > pFrame->Limit) ||
		(Data.Size > (pFrame->Limit - pFrame->Size)) ) {
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"WebSocket Deflater exceeded its advertised output bound",
			NULL
		);
		return false;
	}
	if ( pFrame->Buffer != NULL ) {
		if ( !xrtNetBufAppend(
			pFrame->Buffer,
			Data.Data,
			Data.Size
		) ) {
			(void)__xrtWsConnReject(
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) : XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"write-compressed-websocket-fragment",
				"compressed WebSocket fragment buffer allocation failed",
				xrtGetError()
			);
			return false;
		}
		pFrame->Size += Data.Size;
		return true;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pFrame->Output->Data +
				XWS_FRAME_HEAD_MAX + pFrame->Size,
			Data.Data,
			Data.Size
		);
	}
	pFrame->Size += Data.Size;
	return true;
}



/* 半条压缩消息已经在线路上时，任何后续编码故障都必须终止会话。 */
static void __xrtWsConnDeflatePartFatal(
	xwsconn* pConnection,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ?
			xrtErrorKind(pCause) : XERR_INTERNAL,
		XWS_CONN_ERROR_SEND,
		"write-compressed-websocket-fragment",
		sMessage,
		pCause
	);
	__xrtWsConnEmitError(pConnection);
	(void)xrtWsConnAbort(pConnection);
}



/*
	预留最大编码负载，推进一个同步压缩边界，再把实际负载原地封成最终帧。
	预算、掩码和 OOM 都在 Deflater 状态改变前完成。
*/
xnetresult __xrtWsConnSendDeflatePart(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Input,
	bool bFirst,
	bool bFinal
)
{
	__xrt_ws_deflate_frame Output;
	xnetbuf Buffer;
	xwsframe Frame;
	xwsframeconfig FrameConfig;
	size_t iBound;
	size_t iAllocation;
	size_t iHead = 0;
	size_t iWireSize;
	xnetresult Budget;
	xnetresult Result;
	bool bEncoded;
	bool bBuffer;

	if ( !xrtWsDeflaterBound(Input.Size, &iBound) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"compressed WebSocket fragment bound is not representable",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	Budget = __xrtWsConnFrameBudget(
		pConnection,
		iBound,
		__XRT_WS_SEND_DATA,
		&iWireSize
	);
	if ( Budget != XNET_RESULT_OK ) {
		return Budget;
	}
	if ( iBound >
		(SIZE_MAX - XWS_FRAME_HEAD_MAX -
		 sizeof(*Output.Output)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"compressed WebSocket frame allocation size overflowed",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memset(&Output, 0, sizeof(Output));
	memset(&Buffer, 0, sizeof(Buffer));
	bBuffer = (pConnection->Config.Role == XWS_ROLE_SERVER) &&
		(pConnection->TransportKind == __XRT_WS_TRANSPORT_TCP);
	if ( bBuffer ) {
		if ( !xrtNetBufInit(
			&Buffer,
			xrtNetWorkerBufPool(pConnection->Worker)
		) ) {
			return XNET_RESULT_ERROR;
		}
		Output.Buffer = &Buffer;
	} else {
		iAllocation = sizeof(*Output.Output) +
			XWS_FRAME_HEAD_MAX + iBound;
		Output.Output = (__xrt_ws_output*)xrtMalloc(iAllocation);
		if ( Output.Output == NULL ) {
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"write-compressed-websocket-fragment",
				"compressed WebSocket frame allocation failed",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
		memset(Output.Output, 0, sizeof(*Output.Output));
	}
	Output.Size = 0;
	Output.Limit = iBound;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bFirst ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		Frame.Flags |= XWS_FRAME_MASKED;
		if ( !xrtSecureRandom(Frame.Mask, sizeof(Frame.Mask)) ) {
			xrtFree(Output.Output);
			(void)__xrtWsConnReject(
				XERR_IO,
				XWS_CONN_ERROR_RANDOM,
				"write-compressed-websocket-fragment",
				"WebSocket client could not generate a frame mask",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
	}

	bEncoded = (!bFirst || xrtWsDeflaterBegin(
		pConnection->Deflater,
		true
	)) && xrtWsDeflaterWrite(
		pConnection->Deflater,
		Input,
		__xrtWsConnDeflateFrameOutput,
		&Output
	) && (bFinal ? xrtWsDeflaterEnd(
		pConnection->Deflater,
		__xrtWsConnDeflateFrameOutput,
		&Output
	) : xrtWsDeflaterFlush(
		pConnection->Deflater,
		__xrtWsConnDeflateFrameOutput,
		&Output
	));
	if ( !bEncoded ) {
		xrtFree(Output.Output);
		xrtNetBufClear(&Buffer);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compression failed after a fragmented message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( Output.Buffer != NULL ) {
		Result = __xrtWsConnDeflateBufferSubmit(
			pConnection,
			Opcode,
			&Buffer,
			Output.Size,
			bFirst,
			bFinal
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtNetBufClear(&Buffer);
			if ( bFirst ) {
				__xrtWsConnDeflateRollback(pConnection);
			} else {
				__xrtWsConnDeflatePartFatal(
					pConnection,
					"WebSocket transport rejected a compressed fragmented message"
				);
				Result = XNET_RESULT_ERROR;
			}
		}
		return Result;
	}

	Frame.PayloadSize = Output.Size;
	xrtWsFrameConfigInit(&FrameConfig);
	FrameConfig.AllowedRsv = XWS_FRAME_RSV1;
	if ( !xrtWsFrameWrite(
		&Frame,
		&FrameConfig,
		NULL,
		0,
		&iHead
	) || (iHead > XWS_FRAME_HEAD_MAX) ) {
		xrtFree(Output.Output);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compressed frame header failed after a message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( Output.Size != 0 ) {
		memmove(
			Output.Output->Data + iHead,
			Output.Output->Data + XWS_FRAME_HEAD_MAX,
			Output.Size
		);
	}
	if ( !xrtWsFrameWrite(
		&Frame,
		&FrameConfig,
		Output.Output->Data,
		iHead,
		&iHead
	) ) {
		xrtFree(Output.Output);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compressed frame construction failed after a message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( ((Frame.Flags & XWS_FRAME_MASKED) != 0) &&
		(Output.Size != 0) ) {
		(void)xrtWsMask(
			Output.Output->Data + iHead,
			Output.Size,
			Frame.Mask,
			0
		);
	}
	Output.Output->Size = iHead + Output.Size;
	Result = __xrtWsConnSubmit(
		pConnection,
		Output.Output,
		__XRT_WS_SEND_DATA
	);
	if ( Result != XNET_RESULT_OK ) {
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket transport rejected a compressed fragmented message"
			);
			Result = XNET_RESULT_ERROR;
		}
	}
	return Result;
}
#endif



/* 压缩、封成单帧并仅在传输受理后提交上下文。 */
static xnetresult __xrtWsConnSendDeflate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	__xrt_ws_compressed Output;
	xnetbuf Buffer;
	xnetresult Result;
	size_t iMinimum;
	size_t iAvailable = __xrtWsConnAvailable(
		pConnection,
		__XRT_WS_SEND_DATA
	);
	size_t iCapacity = __xrtWsConnCapacity(
		pConnection,
		__XRT_WS_SEND_DATA
	);

	if ( iCapacity == 0 ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket connection has no permanent data send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnFrameSize(
		pConnection,
		0,
		&iMinimum
	) || !__xrtWsConnTransportSize(
		pConnection,
		iMinimum,
		&iMinimum
	) ) {
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket minimum compressed frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iAvailable < iMinimum ) {
		__xrtWsConnBackpressure(pConnection);
		return XNET_RESULT_AGAIN;
	}
	memset(&Output, 0, sizeof(Output));
	(void)xrtNetBufInit(&Buffer, NULL);
	if ( (pConnection->Config.Role == XWS_ROLE_SERVER) &&
		(pConnection->TransportKind == __XRT_WS_TRANSPORT_TCP) ) {
		if ( !xrtNetBufInit(
			&Buffer,
			xrtNetWorkerBufPool(pConnection->Worker)
		) ) {
			return XNET_RESULT_ERROR;
		}
		Output.Buffer = &Buffer;
	}
	Output.Limit = iCapacity;
	if ( (uint64)Output.Limit >
		pConnection->Config.FrameLimit ) {
		Output.Limit = (size_t)
			pConnection->Config.FrameLimit;
	}
	if ( !xrtWsDeflaterBegin(
		pConnection->Deflater,
		true
	) || !xrtWsDeflaterWrite(
		pConnection->Deflater,
		Payload,
		__xrtWsConnDeflateOutput,
		&Output
	) || !xrtWsDeflaterEnd(
		pConnection->Deflater,
		__xrtWsConnDeflateOutput,
		&Output
	) ) {
		const xerror* pCause = xrtGetError();

		if ( (pCause == NULL) ||
			(xrtErrorDomain(pCause) == NULL) ||
			(strcmp(
				xrtErrorDomain(pCause),
				"xrt.websocket.connection"
			 ) != 0) ) {
			(void)__xrtWsConnReject(
				pCause != NULL ?
					xrtErrorKind(pCause) :
					XERR_INTERNAL,
				XWS_CONN_ERROR_MESSAGE,
				"compress-websocket-message",
				"WebSocket message compression failed",
				pCause
			);
		}
		xrtFree(Output.Data);
		xrtNetBufClear(&Buffer);
		__xrtWsConnDeflateRollback(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( Output.Buffer != NULL ) {
		Result = __xrtWsConnDeflateBufferSubmit(
			pConnection,
			Opcode,
			&Buffer,
			Output.Size,
			true,
			true
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtNetBufClear(&Buffer);
			__xrtWsConnDeflateRollback(pConnection);
		}
		return Result;
	}
	Result = __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		(xbytesview) {
			Output.Data,
			Output.Size
		},
		true,
		__XRT_WS_SEND_DATA,
		true
	);
	xrtFree(Output.Data);
	if ( Result != XNET_RESULT_OK ) {
		__xrtWsConnDeflateRollback(pConnection);
	}
	return Result;
}
#endif



/* 验证一条完整 Text 或 Binary 消息的不可变参数。 */
bool __xrtWsConnMessageCheck(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	size_t iWireSize;

	if ( (Opcode != XWS_OPCODE_TEXT) &&
		(Opcode != XWS_OPCODE_BINARY) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-message",
			"WebSocket message opcode must be Text or Binary",
			NULL
		);
		return false;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-message",
			"WebSocket message range is invalid",
			NULL
		);
		return false;
	}
	if ( (Payload.Size >
		 pConnection->Config.MessageLimit) ||
		(!bCompressed &&
		 ((uint64)Payload.Size >
		  pConnection->Config.FrameLimit)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-message",
			"WebSocket message exceeds its configured limit",
			NULL
		);
		return false;
	}
	if ( Opcode == XWS_OPCODE_TEXT ) {
		xstrview Text = {
			(const char*)Payload.Data,
			Payload.Size
		};

		if ( !xrtUtf8Valid(Text, NULL) ) {
			(void)__xrtWsConnReject(
				XERR_VALUE,
				XWS_CONN_ERROR_MESSAGE,
				"send-websocket-message",
				"WebSocket text message is not valid UTF-8",
				xrtGetError()
			);
			return false;
		}
	}
	if ( !bCompressed &&
		(!__xrtWsConnFrameSize(
			pConnection,
			Payload.Size,
			&iWireSize
		 ) || (iWireSize >
			__xrtWsConnCapacity(
				pConnection,
				__XRT_WS_SEND_DATA
			))) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-message",
			"WebSocket message exceeds its permanent send capacity",
			NULL
		);
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( bCompressed ) {
			if ( !pConnection->Config.DeflateEnabled ) {
				(void)__xrtWsConnReject(
					XERR_STATE,
					XWS_CONN_ERROR_CONFIG,
					"compress-websocket-message",
					"WebSocket connection did not negotiate compression",
					NULL
				);
				return false;
			}
		}
	#else
		(void)bCompressed;
	#endif
	return true;
}



/* 验证并发送一条完整 Text 或 Binary 消息。 */
static xnetresult __xrtWsConnSendMessage(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	if ( !__xrtWsConnWorker(
		pConnection,
		"send-websocket-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		if ( pConnection->Writer != NULL ) {
			return XNET_RESULT_AGAIN;
		}
	#endif
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		bCompressed
	) ) {
		return XNET_RESULT_ERROR;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( bCompressed ) {
			return __xrtWsConnSendDeflate(
				pConnection,
				Opcode,
				Payload
			);
		}
	#endif
	return __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XRT_WS_SEND_DATA,
		false
	);
}



/* 发送一条完整 Text 或 Binary 消息。 */
XRT_API xnetresult xrtWsConnSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendMessage(
		pConnection,
		Opcode,
		Payload,
		false
	);
}



/* 发送完整 Text。 */
XRT_API xnetresult xrtWsConnText(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSend(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 发送完整 Binary。 */
XRT_API xnetresult xrtWsConnBinary(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSend(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 压缩并发送完整 Text 或 Binary。 */
XRT_API xnetresult xrtWsConnSendCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendMessage(
		pConnection,
		Opcode,
		Payload,
		true
	);
}



/* 压缩并发送完整 Text。 */
XRT_API xnetresult xrtWsConnTextCompressed(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendCompressed(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 压缩并发送完整 Binary。 */
XRT_API xnetresult xrtWsConnBinaryCompressed(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendCompressed(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}
#endif



/* 在所属 Worker 上发送一条 Ping 或 Pong 控制帧。 */
static xnetresult __xrtWsConnControlSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	cstr sOperation = Opcode == XWS_OPCODE_PING ?
		"send-websocket-ping" :
		"send-websocket-pong";

	if ( !__xrtWsConnWorker(
		pConnection,
		sOperation
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	return __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XRT_WS_SEND_CONTROL,
		false
	);
}



/* 发送 Ping 控制帧。 */
XRT_API xnetresult xrtWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlSend(
		pConnection,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 发送 Pong 控制帧。 */
XRT_API xnetresult xrtWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlSend(
		pConnection,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 发送唯一 Close 并等待远端回应。 */
XRT_API xnetresult xrtWsConnClose(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iSize = 0;

	if ( !__xrtWsConnWorker(
		pConnection,
		"close-websocket"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pConnection->CloseSent ||
		(xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"close-websocket",
			"WebSocket Close code or reason is invalid",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtWsConnClosePayload(
		pConnection,
		(xbytesview) { Payload, iSize },
		iCode,
		false
	);
}



/* 从任意线程安全取得临时传输引用并请求异常关闭。 */
XRT_API bool xrtWsConnAbort(xwsconn* pConnection)
{
	ptr pTransport;
	bool bAccepted;
	uint32 iState;

	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"abort-websocket"
	) ) {
		return false;
	}
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	while ( iState == XWS_CONN_OPEN ) {
		if ( xrtAtomic32CompareExchange(
			&pConnection->State,
			&iState,
			XWS_CONN_CLOSING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	if ( iState == XWS_CONN_CLOSED ) {
		return false;
	}
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			bAccepted = xrtTlsStreamAbort(
				(xtlsstream*)pTransport
			);
		} else
	#endif
	{
		bAccepted = xrtNetStreamAbort(
			(xnetstream*)pTransport
		);
	}
	__xrtWsConnTransportRelease(
		pConnection,
		pTransport
	);
	return bAccepted;
}



/* 复制 Close 快照。 */
XRT_API bool xrtWsConnCloseInfo(
	const xwsconn* pConnection,
	xwsconnclose* pClose
)
{
	size_t iConnectionSize;

	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-close"
	) ) {
		return false;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnectionSize
	) || !xrtMemRangeValid(pClose, sizeof(*pClose)) ||
		xrtMemRangesOverlap(
			pClose,
			sizeof(*pClose),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"query-websocket-close",
			"WebSocket connection or disjoint Close output is invalid",
			NULL
		);
		return false;
	}
	__xrtWsConnCloseSnapshot(pConnection, pClose);
	return true;
}



/* 返回第一个结构化错误。 */
XRT_API const xerror* xrtWsConnError(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-error"
	) ) {
		return NULL;
	}
	return (const xerror*)xrtAtomicPtrLoad(
		&pConnection->Error,
		XMEMORY_ACQUIRE
	);
}

#endif
