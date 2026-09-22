#include <xrt.h>
#include "../internal/xrt_websocket_group.h"
#include "../internal/xrt_websocket_group_future.h"

#include "../../include/xrt/websocket_group.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)

typedef struct __xrt_ws_group_item __xrt_ws_group_item;



/* 批量操作的每个槽位保存提交结果和一个无额外分配的完成监听节点。 */
struct __xrt_ws_group_item {
	struct xwsgroupop* Operation;
	xfuture* Future;
	xerror* Error;
	xfuturewatch Watch;
	bool Completed;
};



/* 批量操作用一次连续分配保存稳定快照之外的全部槽位状态。 */
struct xwsgroupop {
	volatile int32 References;
	xmutex Lock;
	xwsgroupsnapshot* Snapshot;
	xfuture* Completion;
	xpromise* Promise;
	xcancelwatch* Watch;
	size_t Count;
	size_t Accepted;
	size_t Rejected;
	size_t Remaining;
	bool Submitting;
	__xrt_ws_group_item Items[];
};



/* 验证批量操作头、槽位乘法和完整连续分配区间。 */
static bool __xrtWsGroupOpCheck(
	const xwsgroupop* pOperation,
	cstr sOperation,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xrtMemRangeValid(pOperation, sizeof(*pOperation)) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group operation range is invalid"
		);
		return false;
	}
	if ( (pOperation->Count > (size_t)(INT32_MAX - 2)) ||
		(pOperation->Count >
		 ((SIZE_MAX - sizeof(*pOperation)) /
		  sizeof(__xrt_ws_group_item))) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			sOperation,
			"WebSocket group operation size overflows"
		);
		return false;
	}
	iSize = sizeof(*pOperation) +
		(pOperation->Count * sizeof(__xrt_ws_group_item));
	if ( !xrtMemRangeValid(pOperation, iSize) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group operation storage is incomplete"
		);
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 前置声明批量操作内部引用释放路径。 */
static void __xrtWsGroupOpRelease(xwsgroupop* pOperation);



/* 释放完成监听或调用方持有的一个批量操作内部引用。 */
static void __xrtWsGroupOpRelease(xwsgroupop* pOperation)
{
	if ( xrtRefRelease(&pOperation->References) != 0 ) {
		return;
	}
	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		xrtFutureDestroy(pOperation->Items[i].Future);
		xrtErrorFree(pOperation->Items[i].Error);
	}
	if ( pOperation->Promise != NULL ) {
		xrtPromiseDestroy(pOperation->Promise);
	}
	xrtFutureDestroy(pOperation->Completion);
	xrtWsGroupSnapshotDestroy(pOperation->Snapshot);
	(void)xrtMutexUnit(&pOperation->Lock);
	xrtFree(pOperation);
}



/* 完成监听离开源 Future 后归还它持有的操作引用。 */
static void __xrtWsGroupOpWaiterRelease(ptr pData)
{
	__xrt_ws_group_item* pItem = (__xrt_ws_group_item*)pData;

	__xrtWsGroupOpRelease(pItem->Operation);
}



/* 在最后一个已接纳 Future 结束后取得唯一完成 Promise。 */
static xpromise* __xrtWsGroupOpFinishLocked(
	xwsgroupop* pOperation,
	xcancelwatch** ppWatch
)
{
	xpromise* pPromise;

	if ( pOperation->Submitting ||
		(pOperation->Remaining != 0) ||
		(pOperation->Promise == NULL) ) {
		return NULL;
	}
	pPromise = pOperation->Promise;
	pOperation->Promise = NULL;
	*ppWatch = pOperation->Watch;
	pOperation->Watch = NULL;
	return pPromise;
}



/* 发布完成 Future 的唯一成功终态。 */
static void __xrtWsGroupOpResolve(
	xwsgroupop* pOperation,
	xpromise* pPromise,
	xcancelwatch* pWatch
)
{
	if ( pPromise == NULL ) {
		return;
	}
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
		__xrtWsGroupOpRelease(pOperation);
	}
	(void)xrtPromiseResolve(pPromise, NULL);
	xrtPromiseDestroy(pPromise);
}



/* 一个逐成员 Future 进入终态后更新剩余计数。 */
static void __xrtWsGroupOpSourceDone(ptr pData)
{
	__xrt_ws_group_item* pItem = (__xrt_ws_group_item*)pData;
	xwsgroupop* pOperation = pItem->Operation;
	xpromise* pPromise = NULL;
	xcancelwatch* pWatch = NULL;

	(void)xrtMutexLock(&pOperation->Lock);
	if ( !pItem->Completed ) {
		pItem->Completed = true;
		pOperation->Remaining--;
		pPromise = __xrtWsGroupOpFinishLocked(
			pOperation,
			&pWatch
		);
	}
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
}



/* 保存同步提交失败，并从当前执行上下文取走精确错误。 */
static void __xrtWsGroupOpReject(__xrt_ws_group_item* pItem)
{
	pItem->Error = xrtTakeError();
	if ( pItem->Error == NULL ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.submit",
			"WebSocket group member rejected an operation without an error"
		);
		pItem->Error = xrtTakeError();
	}
}



/* 为已接纳 Future 注册预分配完成监听。 */
static void __xrtWsGroupOpAccept(
	xwsgroupop* pOperation,
	__xrt_ws_group_item* pItem,
	xfuture* pFuture
)
{
	xfuturewatchresult WatchResult;
	xpromise* pPromise = NULL;
	xcancelwatch* pWatch = NULL;

	pItem->Operation = pOperation;
	pItem->Future = pFuture;
	if ( !xrtFutureWatchInit(
		&pItem->Watch,
		__xrtWsGroupOpSourceDone,
		__xrtWsGroupOpWaiterRelease,
		pItem
	) ) {
		__xrtWsGroupOpReject(pItem);
		xrtFutureDestroy(pFuture);
		pItem->Future = NULL;
		(void)xrtMutexLock(&pOperation->Lock);
		pOperation->Rejected++;
		(void)xrtMutexUnlock(&pOperation->Lock);
		return;
	}
	(void)xrtMutexLock(&pOperation->Lock);
	pOperation->Accepted++;
	pOperation->Remaining++;
	(void)xrtMutexUnlock(&pOperation->Lock);
	(void)xrtRefRetain(&pOperation->References);
	WatchResult = xrtFutureWatchAdd(pFuture, &pItem->Watch);
	if ( WatchResult == XFUTURE_WATCH_PENDING ) {
		return;
	}
	if ( WatchResult == XFUTURE_WATCH_READY ) {
		__xrtWsGroupOpSourceDone(pItem);
		__xrtWsGroupOpRelease(pOperation);
		return;
	}

	/* Watch 注册失败属于同步拒绝，不能伪装成成员 Future 已完成。 */
	__xrtWsGroupOpReject(pItem);
	xrtFutureDestroy(pFuture);
	pItem->Future = NULL;
	(void)xrtMutexLock(&pOperation->Lock);
	pItem->Completed = true;
	pOperation->Accepted--;
	pOperation->Rejected++;
	pOperation->Remaining--;
	pPromise = __xrtWsGroupOpFinishLocked(pOperation, &pWatch);
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
	__xrtWsGroupOpRelease(pOperation);
}



/* 创建尚未提交成员操作、但已经具备稳定快照和完成 Future 的对象。 */
xwsgroupop* __xrtWsGroupOpCreate(xwsgroup* pGroup)
{
	xwsgroupop* pOperation;
	xwsgroupsnapshot* pSnapshot;
	size_t iCount;
	size_t iBytes;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.operation"
	) ) {
		return NULL;
	}
	pSnapshot = xrtWsGroupSnapshotCreate(pGroup);
	if ( pSnapshot == NULL ) {
		return NULL;
	}
	iCount = xrtWsGroupSnapshotCount(pSnapshot);
	if ( (iCount > (size_t)(INT32_MAX - 2)) ||
		(iCount > ((SIZE_MAX - sizeof(*pOperation)) /
		 sizeof(__xrt_ws_group_item))) ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation",
			"WebSocket group operation size overflows"
		);
		return NULL;
	}
	iBytes = sizeof(*pOperation) +
		(iCount * sizeof(__xrt_ws_group_item));
	pOperation = (xwsgroupop*)xrtCalloc(1, iBytes);
	if ( pOperation == NULL ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation",
			"WebSocket group operation allocation failed"
		);
		return NULL;
	}
	pOperation->References = 1;
	pOperation->Snapshot = pSnapshot;
	pOperation->Count = iCount;
	pOperation->Submitting = true;
	if ( !xrtMutexInit(&pOperation->Lock) ) {
		xrtWsGroupSnapshotDestroy(pSnapshot);
		xrtFree(pOperation);
		return NULL;
	}
	pOperation->Promise = xrtPromiseCreate(
		&pOperation->Completion,
		NULL
	);
	if ( pOperation->Promise == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation",
			"WebSocket group completion Future allocation failed"
		);
		__xrtWsGroupOpRelease(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 完成 Future 被请求取消时立即取消聚合等待，并把请求传播给成员。 */
static void __xrtWsGroupOpCancelled(ptr pData)
{
	xwsgroupop* pOperation = (xwsgroupop*)pData;
	xpromise* pPromise;
	xcancelwatch* pWatch;

	(void)xrtMutexLock(&pOperation->Lock);
	if ( pOperation->Promise == NULL ) {
		(void)xrtMutexUnlock(&pOperation->Lock);
		return;
	}
	pPromise = pOperation->Promise;
	pOperation->Promise = NULL;
	pWatch = pOperation->Watch;
	pOperation->Watch = NULL;
	(void)xrtMutexUnlock(&pOperation->Lock);

	(void)xrtWsGroupOpCancel(pOperation);
	(void)xrtPromiseCancel(pPromise);
	xrtPromiseDestroy(pPromise);
	if ( pWatch != NULL ) {
		xrtCancelUnwatch(pWatch);
		__xrtWsGroupOpRelease(pOperation);
	}
}



/* 在任何成员提交前注册完成 Future 的取消传播监听。 */
static bool __xrtWsGroupOpWatch(xwsgroupop* pOperation)
{
	xcancel* pCancel = xrtPromiseCancelToken(pOperation->Promise);

	if ( pCancel == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation-watch",
			"WebSocket group completion cancel token failed"
		);
		return false;
	}
	(void)xrtRefRetain(&pOperation->References);
	pOperation->Watch = xrtCancelWatch(
		pCancel,
		__xrtWsGroupOpCancelled,
		pOperation
	);
	xrtCancelDestroy(pCancel);
	if ( pOperation->Watch == NULL ) {
		__xrtWsGroupOpRelease(pOperation);
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.operation-watch",
			"WebSocket group completion cancel watch failed"
		);
		return false;
	}
	return true;
}



/* 遍历稳定快照，保留每个成员的接纳 Future 或同步拒绝错误。 */
bool __xrtWsGroupOpSubmit(
	xwsgroupop* pOperation,
	__xrt_ws_group_submitproc pSubmit,
	ptr pData
)
{
	xpromise* pPromise;
	xcancelwatch* pWatch = NULL;

	if ( !__xrtWsGroupOpWatch(pOperation) ) {
		return false;
	}

	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		__xrt_ws_group_item* pItem = &pOperation->Items[i];
		xwsconn* pConnection = xrtWsGroupSnapshotGet(
			pOperation->Snapshot,
			i
		);
		xfuture* pFuture = pSubmit(pConnection, pData);

		if ( pFuture == NULL ) {
			pOperation->Rejected++;
			__xrtWsGroupOpReject(pItem);
		} else {
			__xrtWsGroupOpAccept(
				pOperation,
				pItem,
				pFuture
			);
		}
	}
	(void)xrtMutexLock(&pOperation->Lock);
	pOperation->Submitting = false;
	pPromise = __xrtWsGroupOpFinishLocked(
		pOperation,
		&pWatch
	);
	(void)xrtMutexUnlock(&pOperation->Lock);
	__xrtWsGroupOpResolve(pOperation, pPromise, pWatch);
	return true;
}



/* 增加批量操作引用并返回原指针。 */
XRT_API xwsgroupop* xrtWsGroupOpRef(xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-ref",
		NULL
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pOperation->References) < 0 ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.operation-ref",
			"WebSocket group operation reference cannot be retained"
		);
		return NULL;
	}
	return pOperation;
}



/* 释放批量操作引用。 */
XRT_API void xrtWsGroupOpDestroy(xwsgroupop* pOperation)
{
	if ( pOperation == NULL ) {
		return;
	}
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-destroy",
		NULL
	) ) {
		return;
	}
	__xrtWsGroupOpRelease(pOperation);
}



/* 返回稳定成员槽位总数。 */
XRT_API size_t xrtWsGroupOpCount(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-count",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Count;
}



/* 返回成功创建逐成员 Future 的槽位数。 */
XRT_API size_t xrtWsGroupOpAccepted(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-accepted",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Accepted;
}



/* 返回提交阶段同步拒绝的槽位数。 */
XRT_API size_t xrtWsGroupOpRejected(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-rejected",
		NULL
	) ) {
		return 0;
	}
	return pOperation->Rejected;
}



/* 返回同步拒绝和已经进入 Future 终态的槽位数。 */
XRT_API size_t xrtWsGroupOpDoneCount(const xwsgroupop* pOperation)
{
	size_t iDone;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-done",
		NULL
	) ) {
		return 0;
	}
	if ( !xrtMutexLock((xmutex*)&pOperation->Lock) ) {
		return 0;
	}
	iDone = pOperation->Rejected +
		(pOperation->Accepted - pOperation->Remaining);
	(void)xrtMutexUnlock((xmutex*)&pOperation->Lock);
	return iDone;
}



/* 把通用 Future 状态映射为批量操作槽位状态。 */
static xwsgroupopstate __xrtWsGroupOpState(xfuturestate State)
{
	switch ( State ) {
		case XFUTURE_RESOLVED:
			return XWS_GROUP_OP_RESOLVED;
		case XFUTURE_FAILED:
			return XWS_GROUP_OP_FAILED;
		case XFUTURE_CANCELLED:
			return XWS_GROUP_OP_CANCELLED;
		case XFUTURE_CLOSED:
			return XWS_GROUP_OP_CLOSED;
		default:
			return XWS_GROUP_OP_PENDING;
	}
}



/* 读取一个槽位的稳定成员和当前状态快照。 */
XRT_API bool xrtWsGroupOpResult(
	const xwsgroupop* pOperation,
	size_t iIndex,
	xwsgroupopresult* pResult
)
{
	const __xrt_ws_group_item* pItem;
	xwsgroupopresult Result;
	xfuturestate State;
	size_t iOperationSize;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-result",
		&iOperationSize
	) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pResult, sizeof(Result)) ||
		xrtMemRangesOverlap(
			pResult,
			sizeof(Result),
			pOperation,
			iOperationSize
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.operation-result",
			"WebSocket group operation result range is invalid"
		);
		return false;
	}
	if ( iIndex >= pOperation->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation-result",
			"WebSocket group operation index is out of range"
		);
		return false;
	}
	pItem = &pOperation->Items[iIndex];
	Result.Connection = xrtWsGroupSnapshotGet(
		pOperation->Snapshot,
		iIndex
	);
	if ( pItem->Future == NULL ) {
		Result.State = XWS_GROUP_OP_REJECTED;
		Result.Error = pItem->Error;
		memcpy(pResult, &Result, sizeof(Result));
		return true;
	}
	State = xrtFutureState(pItem->Future);
	Result.State = __xrtWsGroupOpState(State);
	Result.Error = State == XFUTURE_FAILED ?
		xrtFutureError(pItem->Future) : NULL;
	memcpy(pResult, &Result, sizeof(Result));
	return true;
}



/* 返回增加引用后的逐成员 Future。 */
XRT_API xfuture* xrtWsGroupOpItemFutureRef(
	const xwsgroupop* pOperation,
	size_t iIndex
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-item",
		NULL
	) ) {
		return NULL;
	}
	if ( iIndex >= pOperation->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.operation-item",
			"WebSocket group operation index is out of range"
		);
		return NULL;
	}
	if ( pOperation->Items[iIndex].Future == NULL ) {
		return NULL;
	}
	return xrtFutureRef(pOperation->Items[iIndex].Future);
}



/* 返回增加引用后的完成 Future。 */
XRT_API xfuture* xrtWsGroupOpFutureRef(const xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-future",
		NULL
	) ) {
		return NULL;
	}
	return xrtFutureRef(pOperation->Completion);
}



/* 向全部仍等待的逐成员 Future 请求协作取消。 */
XRT_API size_t xrtWsGroupOpCancel(xwsgroupop* pOperation)
{
	size_t iCancelled = 0;

	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-cancel",
		NULL
	) ) {
		return 0;
	}
	for ( size_t i = 0; i < pOperation->Count; i++ ) {
		if ( (pOperation->Items[i].Future != NULL) &&
			xrtFutureCancel(pOperation->Items[i].Future) ) {
			iCancelled++;
		}
	}
	return iCancelled;
}



/* 等待全部已接纳操作进入终态。 */
XRT_API xwaitresult xrtWsGroupOpWait(xwsgroupop* pOperation)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWait(pOperation->Completion);
}



/* 在相对微秒数内等待全部已接纳操作进入终态。 */
XRT_API xwaitresult xrtWsGroupOpWaitFor(
	xwsgroupop* pOperation,
	uint64 iTimeout
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWaitFor(pOperation->Completion, iTimeout);
}



/* 等待全部已接纳操作到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	xdeadline iDeadline
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWaitUntil(pOperation->Completion, iDeadline);
}



/* 等待批量操作、截止时间或调用方取消令牌中的首个事件。 */
XRT_API xwaitresult xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtWsGroupOpCheck(
		pOperation,
		"websocket-group.operation-wait",
		NULL
	) ) {
		return XWAIT_ERROR;
	}
	return xrtFutureWaitUntilCancel(
		pOperation->Completion,
		iDeadline,
		pCancel
	);
}

#endif
