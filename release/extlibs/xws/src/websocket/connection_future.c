#include "../internal/xrt_websocket.h"



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)

/* 每个异步节点只表达一种发送或观察操作。 */
typedef enum __xrt_ws_async_kind {
	__XRT_WS_ASYNC_MESSAGE = 1,
	__XRT_WS_ASYNC_MESSAGE_COMPRESSED,
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		__XRT_WS_ASYNC_MESSAGE_REF,
	#endif
	__XRT_WS_ASYNC_CONTROL,
	__XRT_WS_ASYNC_CLOSE,
	__XRT_WS_ASYNC_WAIT
} __xrt_ws_async_kind;



/* Worker 从待定状态线性化到唯一 Future 终态。 */
typedef enum __xrt_ws_async_result {
	__XRT_WS_ASYNC_PENDING = 0,
	__XRT_WS_ASYNC_READY,
	__XRT_WS_ASYNC_FAILED,
	__XRT_WS_ASYNC_CANCELLED,
	__XRT_WS_ASYNC_CLOSED
} __xrt_ws_async_result;



/* 标记节点当前所属队列，支持取消回调常数时间迁移。 */
typedef enum __xrt_ws_async_queue {
	__XRT_WS_ASYNC_QUEUE_NONE = 0,
	__XRT_WS_ASYNC_QUEUE_SEND,
	__XRT_WS_ASYNC_QUEUE_WAIT
} __xrt_ws_async_queue;



/* 受阻后依次寻找控制帧和最终 Close，保持协议关闭始终可推进。 */
typedef enum __xrt_ws_async_take {
	__XRT_WS_ASYNC_TAKE_FIFO = 0,
	__XRT_WS_ASYNC_TAKE_CONTROL,
	__XRT_WS_ASYNC_TAKE_CLOSE
} __xrt_ws_async_take;



/*
	节点、Promise 和复制负载采用单一分配。
	节点持有 Connection，直到 Worker 或同步终态路径完成 Promise。
*/
struct __xrt_ws_async {
	__xrt_ws_async* Next;
	__xrt_ws_async* Previous;
	xwsconn* Connection;
	xpromise* Promise;
	xcancelwatch* Watch;
	size_t Size;
	uint16 CloseCode;
	uint8 Kind;
	uint8 Opcode;
	uint8 Wait;
	uint8 Queue;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		xnetref Ref;
		bool RefOwned;
	#endif
	uint8 Data[];
};



/* 验证公开异步入口收到的完整 Connection 固定结构范围。 */
static bool __xrtWsAsyncConnectionCheck(
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



/* 返回节点是否可以越过暂时受阻的数据消息使用控制预算。 */
static bool __xrtWsAsyncIsControl(
	const __xrt_ws_async* pAsync
)
{
	return (pAsync->Kind == __XRT_WS_ASYNC_CONTROL) ||
		(pAsync->Kind == __XRT_WS_ASYNC_CLOSE);
}



/* 释放一次尚未挂入节点的原子预算预留。 */
static void __xrtWsAsyncBudgetRelease(
	xwsconn* pConnection,
	size_t iBytes
)
{
	if ( iBytes != 0 ) {
		(void)xrtAtomic64FetchSub(
			&pConnection->AsyncBytes,
			(uint64)iBytes,
			XMEMORY_ACQ_REL
		);
	}
	(void)xrtAtomic32FetchSub(
		&pConnection->AsyncCount,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 为一个待定操作原子保留操作数和负载字节硬预算。 */
static bool __xrtWsAsyncReserve(
	xwsconn* pConnection,
	size_t iBytes
)
{
	uint32 iCount = xrtAtomic32Load(
		&pConnection->AsyncCount,
		XMEMORY_ACQUIRE
	);
	uint64 iQueued;

	if ( iBytes > pConnection->Config.AsyncBytesLimit ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"submit-websocket-async",
			"WebSocket asynchronous payload exceeds its hard byte limit",
			NULL
		);
		return false;
	}
	for ( ;; ) {
		uint32 iExpected = iCount;

		if ( iCount >= pConnection->Config.AsyncCountLimit ) {
			(void)__xrtWsConnReject(
				XERR_AGAIN,
				XWS_CONN_ERROR_LIMIT,
				"submit-websocket-async",
				"WebSocket asynchronous operation queue is full",
				NULL
			);
			return false;
		}
		if ( xrtAtomic32CompareExchange(
			&pConnection->AsyncCount,
			&iExpected,
			iCount + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iCount = iExpected;
	}
	iQueued = xrtAtomic64Load(
		&pConnection->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	for ( ;; ) {
		uint64 iExpected = iQueued;
		uint64 iLimit =
			(uint64)pConnection->Config.AsyncBytesLimit;

		if ( (iQueued > iLimit) ||
			((uint64)iBytes > (iLimit - iQueued)) ) {
			__xrtWsAsyncBudgetRelease(
				pConnection,
				0
			);
			(void)__xrtWsConnReject(
				XERR_AGAIN,
				XWS_CONN_ERROR_LIMIT,
				"submit-websocket-async",
				"WebSocket asynchronous payload queue is full",
				NULL
			);
			return false;
		}
		if ( xrtAtomic64CompareExchange(
			&pConnection->AsyncBytes,
			&iExpected,
			iQueued + (uint64)iBytes,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			return true;
		}
		iQueued = iExpected;
	}
}



/* 在节点离开全部内部队列后释放其异步硬预算。 */
static void __xrtWsAsyncRelease(__xrt_ws_async* pAsync)
{
	__xrtWsAsyncBudgetRelease(
		pAsync->Connection,
		pAsync->Size
	);
}



/* 为缺少底层原因的异步故障建立稳定 Connection 错误。 */
static xerror* __xrtWsAsyncError(
	const __xrt_ws_async* pAsync,
	cstr sOperation,
	cstr sMessage
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pAsync->Connection->TransportResult,
		XMEMORY_ACQUIRE
	);

	return __xrtWsConnErrorCreate(
		Result == XNET_RESULT_TIMEOUT ?
			XERR_TIMEOUT : XERR_IO,
		Result == XNET_RESULT_TIMEOUT ?
			XWS_CONN_ERROR_TIMEOUT : XWS_CONN_ERROR_SEND,
		sOperation,
		sMessage,
		NULL
	);
}



/* 完成唯一 Future 终态，并在唤醒观察者前释放队列预算。 */
static void __xrtWsAsyncFinish(
	__xrt_ws_async* pAsync,
	__xrt_ws_async_result Result,
	xerror* pError
)
{
	const xerror* pFailure = pError;
	xerror* pCreated = NULL;
	xpromise* pPromise = pAsync->Promise;
	xwsconn* pConnection = pAsync->Connection;

	xrtCancelUnwatch(pAsync->Watch);
	pAsync->Watch = NULL;
	__xrtWsAsyncRelease(pAsync);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		if ( pAsync->RefOwned ) {
			pAsync->Ref.Release(
				pAsync->Ref.Context,
				pAsync->Ref.Data,
				pAsync->Ref.Size
			);
			pAsync->RefOwned = false;
		}
	#endif
	if ( (Result == __XRT_WS_ASYNC_FAILED) &&
		(pFailure == NULL) ) {
		pCreated = xrtErrorRef(xrtWsConnError(pConnection));
		pFailure = pCreated;
		if ( pFailure == NULL ) {
			pCreated = __xrtWsAsyncError(
				pAsync,
				"complete-websocket-async",
				"WebSocket asynchronous operation failed"
			);
			pFailure = pCreated;
		}
	}
	xrtWsConnDestroy(pConnection);
	xrtFree(pAsync);

	if ( Result == __XRT_WS_ASYNC_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_WS_ASYNC_FAILED ) {
		if ( pFailure != NULL ) {
			(void)xrtPromiseReject(
				pPromise,
				pFailure
			);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_WS_ASYNC_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}

	xrtErrorFree(pCreated);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 内嵌命令进入 Worker 后允许后续取消再次投递。 */
static void __xrtWsAsyncTask(xnetworker* pWorker, ptr pData)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pWorker;
	xrtSpinLock(&pConnection->AsyncLock);
	pConnection->AsyncPosted = false;
	xrtSpinUnlock(&pConnection->AsyncLock);
	__xrtWsConnFutureNotify(pConnection);
	xrtWsConnDestroy(pConnection);
}



/* 调用方持有 AsyncLock 时无分配地唤醒所属 Worker。 */
static void __xrtWsAsyncSchedule(xwsconn* pConnection)
{
	if ( pConnection->AsyncPosted ) {
		return;
	}
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return;
	}
	pConnection->AsyncPosted = true;
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->AsyncCommand,
		__xrtWsAsyncTask,
		pConnection
	) ) {
		pConnection->AsyncPosted = false;
		xrtWsConnDestroy(pConnection);
	}
}



static void __xrtWsAsyncDetach(
	__xrt_ws_async** ppHead,
	__xrt_ws_async** ppTail,
	__xrt_ws_async* pAsync
);



static void __xrtWsAsyncSendPrepend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
);



/* 取消回调只唤醒 Worker，不从请求取消的线程完成 Promise。 */
static void __xrtWsAsyncCancel(ptr pData)
{
	__xrt_ws_async* pAsync = (__xrt_ws_async*)pData;
	xwsconn* pConnection = pAsync->Connection;

	xrtSpinLock(&pConnection->AsyncLock);
	if ( pAsync->Queue == __XRT_WS_ASYNC_QUEUE_SEND ) {
		__xrtWsAsyncDetach(
			&pConnection->AsyncSendHead,
			&pConnection->AsyncSendTail,
			pAsync
		);
		__xrtWsAsyncSendPrepend(pConnection, pAsync);
		__xrtWsAsyncSchedule(pConnection);
	} else if ( pAsync->Queue == __XRT_WS_ASYNC_QUEUE_WAIT ) {
		__xrtWsAsyncDetach(
			&pConnection->AsyncWaitHead,
			&pConnection->AsyncWaitTail,
			pAsync
		);
		__xrtWsAsyncSendPrepend(pConnection, pAsync);
		__xrtWsAsyncSchedule(pConnection);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
}



/* 从指定双向 FIFO 常数时间摘除节点；调用方持有 AsyncLock。 */
static void __xrtWsAsyncDetach(
	__xrt_ws_async** ppHead,
	__xrt_ws_async** ppTail,
	__xrt_ws_async* pAsync
)
{
	if ( pAsync->Previous != NULL ) {
		pAsync->Previous->Next = pAsync->Next;
	} else {
		*ppHead = pAsync->Next;
	}
	if ( pAsync->Next != NULL ) {
		pAsync->Next->Previous = pAsync->Previous;
	} else {
		*ppTail = pAsync->Previous;
	}
	pAsync->Next = NULL;
	pAsync->Previous = NULL;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_NONE;
}



/* 把发送节点追加到严格 FIFO 队尾；调用方持有 AsyncLock。 */
static void __xrtWsAsyncSendAppend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Previous = pConnection->AsyncSendTail;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_SEND;
	if ( pConnection->AsyncSendTail != NULL ) {
		pConnection->AsyncSendTail->Next = pAsync;
	} else {
		pConnection->AsyncSendHead = pAsync;
	}
	pConnection->AsyncSendTail = pAsync;
}



/* 重试发送必须回到队头，保持已受理异步操作的顺序。 */
static void __xrtWsAsyncSendPrepend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = pConnection->AsyncSendHead;
	pAsync->Previous = NULL;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_SEND;
	if ( pConnection->AsyncSendHead != NULL ) {
		pConnection->AsyncSendHead->Previous = pAsync;
	}
	pConnection->AsyncSendHead = pAsync;
	if ( pConnection->AsyncSendTail == NULL ) {
		pConnection->AsyncSendTail = pAsync;
	}
}



/* 把关闭条件等待追加到独立观察队列。 */
static void __xrtWsAsyncWaitAppend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Previous = pConnection->AsyncWaitTail;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_WAIT;
	if ( pConnection->AsyncWaitTail != NULL ) {
		pConnection->AsyncWaitTail->Next = pAsync;
	} else {
		pConnection->AsyncWaitHead = pAsync;
	}
	pConnection->AsyncWaitTail = pAsync;
}



/* 创建 Promise、取消监听和单一分配节点。 */
static __xrt_ws_async* __xrtWsAsyncCreate(
	xwsconn* pConnection,
	__xrt_ws_async_kind Kind,
	size_t iBytes,
	size_t iStorage,
	xfuture** ppFuture
)
{
	__xrt_ws_async* pAsync;
	xwsconn* pOwned;
	xcancel* pCancel;
	xerror* pError;
	size_t iAllocation;

	*ppFuture = NULL;
	if ( iStorage > (SIZE_MAX - sizeof(*pAsync)) ) {
		__xwsErrorSetSizeOverflow();
		return NULL;
	}
	pOwned = xrtWsConnRef(pConnection);
	if ( pOwned == NULL ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"submit-websocket-async",
			"WebSocket connection reference is no longer valid",
			NULL
		);
		return NULL;
	}
	if ( !__xrtWsAsyncReserve(
		pOwned,
		iBytes
	) ) {
		xrtWsConnDestroy(pOwned);
		return NULL;
	}
	iAllocation = sizeof(*pAsync) + iStorage;
	pAsync = (__xrt_ws_async*)xrtCalloc(1, iAllocation);
	if ( pAsync == NULL ) {
		__xrtWsAsyncBudgetRelease(
			pOwned,
			iBytes
		);
		xrtWsConnDestroy(pOwned);
		return NULL;
	}
	pAsync->Connection = pOwned;
	pAsync->Kind = (uint8)Kind;
	pAsync->Size = iBytes;
	pAsync->Promise = xrtPromiseCreate(ppFuture, NULL);
	if ( pAsync->Promise == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(*ppFuture);
		xrtPromiseDestroy(pAsync->Promise);
		__xrtWsAsyncRelease(pAsync);
		xrtWsConnDestroy(pAsync->Connection);
		xrtFree(pAsync);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		*ppFuture = NULL;
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pAsync->Promise);
	if ( pCancel != NULL ) {
		pAsync->Watch = xrtCancelWatch(
			pCancel,
			__xrtWsAsyncCancel,
			pAsync
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pAsync->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(*ppFuture);
		xrtPromiseDestroy(pAsync->Promise);
		__xrtWsAsyncRelease(pAsync);
		xrtWsConnDestroy(pAsync->Connection);
		xrtFree(pAsync);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		*ppFuture = NULL;
		return NULL;
	}
	return pAsync;
}



/* 关闭后的新操作不依赖已经停止的 Worker，直接进入稳定终态。 */
static __xrt_ws_async_result __xrtWsAsyncClosed(
	const xwsconn* pConnection,
	bool bCloseWait
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pConnection->TransportResult,
		XMEMORY_ACQUIRE
	);

	if ( Result == XNET_RESULT_CANCELLED ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( (xrtWsConnError(pConnection) != NULL) ||
		(Result == XNET_RESULT_ERROR) ||
		(Result == XNET_RESULT_TIMEOUT) ) {
		return __XRT_WS_ASYNC_FAILED;
	}
	return bCloseWait ?
		__XRT_WS_ASYNC_READY : __XRT_WS_ASYNC_CLOSED;
}



/* 把完整节点发布给 Worker，关闭竞争由 AsyncLock 内的状态快照裁决。 */
static void __xrtWsAsyncSubmit(__xrt_ws_async* pAsync)
{
	xwsconn* pConnection = pAsync->Connection;
	bool bClosed;

	xrtSpinLock(&pConnection->AsyncLock);
	bClosed = xrtWsConnState(pConnection) == XWS_CONN_CLOSED;
	if ( !bClosed ) {
		if ( (pAsync->Kind == __XRT_WS_ASYNC_WAIT) &&
			(pAsync->Wait == XWS_CONN_WAIT_CLOSE) ) {
			__xrtWsAsyncWaitAppend(pConnection, pAsync);
		} else {
			/* WRITE 与 DRAIN 留在 FIFO 中，形成只覆盖先前发送的屏障。 */
			__xrtWsAsyncSendAppend(pConnection, pAsync);
		}
		__xrtWsAsyncSchedule(pConnection);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	if ( bClosed ) {
		__xrtWsAsyncFinish(
			pAsync,
			__xrtWsAsyncClosed(
				pConnection,
				pAsync->Kind == __XRT_WS_ASYNC_WAIT &&
				pAsync->Wait == XWS_CONN_WAIT_CLOSE
			),
			NULL
		);
	}
}



/* 普通模式取 FIFO 队首；控制模式取第一个可穿透的控制帧。 */
static __xrt_ws_async* __xrtWsAsyncSendTake(
	xwsconn* pConnection,
	__xrt_ws_async_take Take,
	bool* pbCancelled
)
{
	__xrt_ws_async* pAsync;

	*pbCancelled = false;
	xrtSpinLock(&pConnection->AsyncLock);
	pAsync = pConnection->AsyncSendHead;
	if ( (pAsync != NULL) &&
		xrtCancelTriggered(pAsync->Watch) ) {
		*pbCancelled = true;
	} else if ( Take != __XRT_WS_ASYNC_TAKE_FIFO ) {
		for ( pAsync = pConnection->AsyncSendHead;
			pAsync != NULL; pAsync = pAsync->Next ) {
			if ( (Take == __XRT_WS_ASYNC_TAKE_CONTROL) ?
				__xrtWsAsyncIsControl(pAsync) :
				(pAsync->Kind == __XRT_WS_ASYNC_CLOSE) ) {
				break;
			}
		}
	}
	if ( pAsync != NULL ) {
		*pbCancelled = xrtCancelTriggered(pAsync->Watch);
		__xrtWsAsyncDetach(
			&pConnection->AsyncSendHead,
			&pConnection->AsyncSendTail,
			pAsync
		);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	return pAsync;
}



/* 在 Connection Worker 上执行一次发送尝试。 */
static xnetresult __xrtWsAsyncSend(
	__xrt_ws_async* pAsync
)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		xnetresult Result;

		if ( pAsync->Kind == __XRT_WS_ASYNC_MESSAGE_REF ) {
			Result = xrtWsConnSendRef(
				pAsync->Connection,
				(xwsopcode)pAsync->Opcode,
				&pAsync->Ref
			);
			if ( Result == XNET_RESULT_OK ) {
				pAsync->RefOwned = false;
			}
			return Result;
		}
	#endif
	xbytesview Payload = {
		pAsync->Data,
		pAsync->Size
	};

	if ( pAsync->Kind == __XRT_WS_ASYNC_MESSAGE ) {
		return xrtWsConnSend(
			pAsync->Connection,
			(xwsopcode)pAsync->Opcode,
			Payload
		);
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pAsync->Kind ==
			__XRT_WS_ASYNC_MESSAGE_COMPRESSED ) {
			return xrtWsConnSendCompressed(
				pAsync->Connection,
				(xwsopcode)pAsync->Opcode,
				Payload
			);
		}
	#endif
	if ( pAsync->Kind == __XRT_WS_ASYNC_CONTROL ) {
		if ( pAsync->Opcode == XWS_OPCODE_PING ) {
			return xrtWsConnPing(
				pAsync->Connection,
				Payload
			);
		}
		return xrtWsConnPong(
			pAsync->Connection,
			Payload
		);
	}
	return xrtWsConnClose(
		pAsync->Connection,
		pAsync->CloseCode,
		(xstrview) {
			(const char*)pAsync->Data,
			pAsync->Size
		}
	);
}



/* 把一次发送返回值转换成 Promise 终态。 */
static __xrt_ws_async_result __xrtWsAsyncSendResult(
	const __xrt_ws_async* pAsync,
	xnetresult Result
)
{
	if ( Result == XNET_RESULT_OK ) {
		return __XRT_WS_ASYNC_READY;
	}
	if ( Result == XNET_RESULT_CANCELLED ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( Result == XNET_RESULT_CLOSED ) {
		return __xrtWsAsyncClosed(
			pAsync->Connection,
			false
		);
	}
	return __XRT_WS_ASYNC_FAILED;
}



/* 评估一个条件等待；全部可变状态只由当前 Worker 推进。 */
static __xrt_ws_async_result __xrtWsAsyncWaitResult(
	const __xrt_ws_async* pAsync
)
{
	const xwsconn* pConnection = pAsync->Connection;
	xwsconnstate State = xrtWsConnState(pConnection);

	if ( xrtCancelTriggered(pAsync->Watch) ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( pAsync->Wait == XWS_CONN_WAIT_CLOSE ) {
		return State == XWS_CONN_CLOSED ?
			__xrtWsAsyncClosed(pConnection, true) :
			__XRT_WS_ASYNC_PENDING;
	}
	if ( State == XWS_CONN_CLOSED ) {
		if ( (pAsync->Wait == XWS_CONN_WAIT_DRAIN) &&
			(xrtWsConnError(pConnection) == NULL) &&
			(xrtWsConnPending(pConnection) == 0) ) {
			return __XRT_WS_ASYNC_READY;
		}
		return __xrtWsAsyncClosed(pConnection, false);
	}
	if ( State != XWS_CONN_OPEN ) {
		return __XRT_WS_ASYNC_CLOSED;
	}
	if ( pAsync->Wait == XWS_CONN_WAIT_DRAIN ) {
		return xrtWsConnPending(pConnection) == 0 ?
			__XRT_WS_ASYNC_READY :
			__XRT_WS_ASYNC_PENDING;
	}
	return xrtWsConnWritable(pConnection) != 0 ?
		__XRT_WS_ASYNC_READY :
		__XRT_WS_ASYNC_PENDING;
}



/* 摘除第一个已经满足、取消或终止的条件等待。 */
static __xrt_ws_async* __xrtWsAsyncWaitTake(
	xwsconn* pConnection,
	__xrt_ws_async_result* pResult
)
{
	__xrt_ws_async* pAsync;

	xrtSpinLock(&pConnection->AsyncLock);
	for ( pAsync = pConnection->AsyncWaitHead;
		pAsync != NULL; pAsync = pAsync->Next ) {
		*pResult = __xrtWsAsyncWaitResult(pAsync);
		if ( *pResult != __XRT_WS_ASYNC_PENDING ) {
			__xrtWsAsyncDetach(
				&pConnection->AsyncWaitHead,
				&pConnection->AsyncWaitTail,
				pAsync
			);
			break;
		}
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	return pAsync;
}



/* 依次推进发送 FIFO，再完成观察 Future；同步传输通知折叠到下一批。 */
void __xrtWsConnFutureNotify(xwsconn* pConnection)
{
	uint32 iCompleted = 0;
	uint32 iBatch = pConnection->Config.AsyncBatch;
	__xrt_ws_async_take Take = __XRT_WS_ASYNC_TAKE_FIFO;

	if ( pConnection->AsyncDriving ) {
		pConnection->AsyncNotified = true;
		return;
	}
	pConnection->AsyncDriving = true;
	while ( iCompleted < iBatch ) {
		__xrt_ws_async* pAsync;
		bool bCancelled;
		xnetresult SendResult;
		__xrt_ws_async_result Result;
		xerror* pError;

		pAsync = __xrtWsAsyncSendTake(
			pConnection,
			Take,
			&bCancelled
		);
		if ( pAsync == NULL ) {
			break;
		}
		if ( bCancelled ) {
			__xrtWsAsyncFinish(
				pAsync,
				__XRT_WS_ASYNC_CANCELLED,
				NULL
			);
			iCompleted++;
			continue;
		}
		if ( pAsync->Kind == __XRT_WS_ASYNC_WAIT ) {
			Result = __xrtWsAsyncWaitResult(pAsync);
			if ( Result == __XRT_WS_ASYNC_PENDING ) {
				xrtSpinLock(&pConnection->AsyncLock);
				if ( xrtCancelTriggered(pAsync->Watch) ) {
					Result = __XRT_WS_ASYNC_CANCELLED;
				} else {
					__xrtWsAsyncSendPrepend(
						pConnection,
						pAsync
					);
				}
				xrtSpinUnlock(&pConnection->AsyncLock);
				if ( Result == __XRT_WS_ASYNC_PENDING ) {
					/* 协议控制帧仍可越过暂时受阻的发送屏障。 */
					Take = __XRT_WS_ASYNC_TAKE_CONTROL;
					continue;
				}
			}
			__xrtWsAsyncFinish(pAsync, Result, NULL);
			iCompleted++;
			Take = __XRT_WS_ASYNC_TAKE_FIFO;
			continue;
		}

		xrtClearError();
		SendResult = __xrtWsAsyncSend(pAsync);
		pError = xrtTakeError();
		if ( SendResult == XNET_RESULT_AGAIN ) {
			xrtErrorFree(pError);
			pError = NULL;
			xrtSpinLock(&pConnection->AsyncLock);
			if ( xrtCancelTriggered(pAsync->Watch) ) {
				Result = __XRT_WS_ASYNC_CANCELLED;
			} else if ( xrtWsConnState(pConnection) !=
				XWS_CONN_OPEN ) {
				Result = __xrtWsAsyncClosed(
					pConnection,
					false
				);
			} else {
				__xrtWsAsyncSendPrepend(
					pConnection,
					pAsync
				);
				Result = __XRT_WS_ASYNC_PENDING;
			}
			xrtSpinUnlock(&pConnection->AsyncLock);
			if ( Result == __XRT_WS_ASYNC_PENDING ) {
				if ( pAsync->Kind != __XRT_WS_ASYNC_CLOSE ) {
					Take = __xrtWsAsyncIsControl(pAsync) ?
						__XRT_WS_ASYNC_TAKE_CLOSE :
						__XRT_WS_ASYNC_TAKE_CONTROL;
					continue;
				}
				break;
			}
			__xrtWsAsyncFinish(pAsync, Result, NULL);
			iCompleted++;
			continue;
		}
		Result = __xrtWsAsyncSendResult(
			pAsync,
			SendResult
		);
		__xrtWsAsyncFinish(pAsync, Result, pError);
		iCompleted++;
		Take = __XRT_WS_ASYNC_TAKE_FIFO;
	}

	while ( iCompleted < iBatch ) {
		__xrt_ws_async_result Result =
			__XRT_WS_ASYNC_PENDING;
		__xrt_ws_async* pAsync =
			__xrtWsAsyncWaitTake(
				pConnection,
				&Result
			);

		if ( pAsync == NULL ) {
			break;
		}
		__xrtWsAsyncFinish(pAsync, Result, NULL);
		iCompleted++;
	}
	pConnection->AsyncDriving = false;
	if ( (iCompleted == iBatch) ||
		pConnection->AsyncNotified ) {
		pConnection->AsyncNotified = false;
		xrtSpinLock(&pConnection->AsyncLock);
		if ( (pConnection->AsyncSendHead != NULL) ||
			(pConnection->AsyncWaitHead != NULL) ) {
			__xrtWsAsyncSchedule(pConnection);
		}
		xrtSpinUnlock(&pConnection->AsyncLock);
	}
}



/* 返回当前异步发送负载字节数。 */
XRT_API size_t xrtWsConnAsyncBytes(
	const xwsconn* pConnection
)
{
	uint64 iBytes;

	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"query-websocket-async-bytes"
	) ) {
		return 0;
	}
	iBytes = xrtAtomic64Load(
		&pConnection->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	return iBytes > (uint64)SIZE_MAX ?
		SIZE_MAX : (size_t)iBytes;
}



/* 返回当前异步发送和等待操作总数。 */
XRT_API uint32 xrtWsConnAsyncCount(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"query-websocket-async-count"
	) ) {
		return 0;
	}
	return xrtAtomic32Load(
		&pConnection->AsyncCount,
		XMEMORY_ACQUIRE
	);
}



/* 创建并发布一个条件 Future。 */
XRT_API xfuture* xrtWsConnWaitAsync(
	xwsconn* pConnection,
	xwsconnwait Wait
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"wait-websocket"
	) ) {
		return NULL;
	}
	if ( (Wait < XWS_CONN_WAIT_WRITE) ||
		(Wait > XWS_CONN_WAIT_CLOSE) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"wait-websocket",
			"WebSocket asynchronous wait argument is invalid",
			NULL
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_WAIT,
		0,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Wait = (uint8)Wait;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 预检、复制并发布一个 Text 或 Binary 异步发送。 */
static xfuture* __xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"send-websocket-async"
	) ) {
		return NULL;
	}
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		bCompressed
	) ) {
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		bCompressed ?
			__XRT_WS_ASYNC_MESSAGE_COMPRESSED :
			__XRT_WS_ASYNC_MESSAGE,
		Payload.Size,
		Payload.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Opcode = (uint8)Opcode;
	if ( Payload.Size != 0 ) {
		memcpy(pAsync->Data, Payload.Data, Payload.Size);
	}
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程复制并异步提交完整消息。 */
XRT_API xfuture* xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendAsync(
		pConnection,
		Opcode,
		Payload,
		false
	);
}



/* 从任意线程复制并异步提交 Text。 */
XRT_API xfuture* xrtWsConnTextAsync(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 从任意线程复制并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryAsync(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
/* 预检并发布一条所有权异步消息；仅成功返回 Future 时转移调用方引用。 */
XRT_API xfuture* xrtWsConnSendRefAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	__xrt_ws_async* pAsync;
	xnetref Ref;
	xbytesview Payload;
	xfuture* pFuture;
	size_t iConnection;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"send-websocket-ref-async"
	) ) {
		return NULL;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnection
	) || !xrtMemRangeValid(pRef, sizeof(Ref)) ||
		xrtMemRangesOverlap(
			pRef,
			sizeof(Ref),
			pConnection,
			iConnection
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref-async",
			"WebSocket asynchronous reference range is invalid",
			NULL
		);
		return NULL;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( (Ref.Data == NULL) ||
		(Ref.Size == 0) ||
		(Ref.Release == NULL) ||
		!xrtMemRangeValid(Ref.Data, Ref.Size) ||
		xrtMemRangesOverlap(
			Ref.Data,
			Ref.Size,
			pConnection,
			iConnection
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref-async",
			"WebSocket asynchronous reference is incomplete or invalid",
			NULL
		);
		return NULL;
	}
	Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		false
	) ) {
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_MESSAGE_REF,
		Ref.Size,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Opcode = (uint8)Opcode;
	pAsync->Ref = Ref;
	pAsync->RefOwned = true;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程异步提交所有权 Text。 */
XRT_API xfuture* xrtWsConnTextRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRefAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 从任意线程异步提交所有权 Binary。 */
XRT_API xfuture* xrtWsConnBinaryRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRefAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		pRef
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 从任意线程复制、压缩并异步提交完整消息。 */
XRT_API xfuture* xrtWsConnSendCompressedAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendAsync(
		pConnection,
		Opcode,
		Payload,
		true
	);
}



/* 从任意线程复制、压缩并异步提交 Text。 */
XRT_API xfuture* xrtWsConnTextCompressedAsync(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendCompressedAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 从任意线程复制、压缩并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryCompressedAsync(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendCompressedAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}
#endif



/* 从任意线程复制并异步提交一条 Ping 或 Pong。 */
static xfuture* __xrtWsConnControlAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;
	cstr sOperation = Opcode == XWS_OPCODE_PING ?
		"ping-websocket-async" :
		"pong-websocket-async";
	cstr sMessage = Opcode == XWS_OPCODE_PING ?
		"WebSocket asynchronous Ping payload is invalid" :
		"WebSocket asynchronous Pong payload is invalid";

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		sOperation
	) ) {
		return NULL;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size > XWS_CLOSE_PAYLOAD_MAX) ) {
		(void)__xrtWsConnReject(
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XERR_ARGUMENT : XERR_RANGE,
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XWS_CONN_ERROR_ARGUMENT :
				XWS_CONN_ERROR_LIMIT,
			sOperation,
			sMessage,
			NULL
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_CONTROL,
		Payload.Size,
		Payload.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	if ( Payload.Size != 0 ) {
		memcpy(pAsync->Data, Payload.Data, Payload.Size);
	}
	pAsync->Opcode = (uint8)Opcode;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程复制并异步提交 Ping。 */
XRT_API xfuture* xrtWsConnPingAsync(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlAsync(
		pConnection,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 从任意线程复制并异步提交 Pong。 */
XRT_API xfuture* xrtWsConnPongAsync(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlAsync(
		pConnection,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 从任意线程预检并异步提交唯一 Close。 */
XRT_API xfuture* xrtWsConnCloseAsync(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iPayload = 0;
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"close-websocket-async"
	) ) {
		return NULL;
	}
	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		Payload,
		sizeof(Payload),
		&iPayload
	) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"close-websocket-async",
			"WebSocket Close code or reason is invalid",
			xrtGetError()
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_CLOSE,
		Reason.Size,
		Reason.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->CloseCode = iCode;
	if ( Reason.Size != 0 ) {
		memcpy(pAsync->Data, Reason.Data, Reason.Size);
	}
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}

#endif
