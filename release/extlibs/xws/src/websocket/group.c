#include "../internal/xrt_websocket_group.h"

#include <xrt.h>
#include "../../include/xrt/websocket_group.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP)

/* 连接组以通用集合保存唯一成员，并用互斥锁保护全部结构状态。 */
struct xwsgroup {
	volatile int32 References;
	xmutex Lock;
	xset Connections;
	size_t Limit;
	bool Sealed;
};



/* 快照使用单次分配保存结构和保序引用数组。 */
struct xwsgroupsnapshot {
	size_t Count;
	xwsconn* Connections[];
};



/* 验证 Group 固定结构并设置稳定的 Group 域错误。 */
bool __xrtWsGroupCheck(
	const xwsgroup* pGroup,
	cstr sOperation
)
{
	if ( xrtMemRangeValid(pGroup, sizeof(*pGroup)) ) {
		return true;
	}
	__xrtWsGroupError(
		XERR_ARGUMENT,
		XWS_GROUP_ERROR_ARGUMENT,
		sOperation,
		"WebSocket group range is invalid"
	);
	return false;
}



/* 判断一个已验证范围是否覆盖 Group 的私有固定结构。 */
bool __xrtWsGroupOverlaps(
	const xwsgroup* pGroup,
	cbytes pData,
	size_t iSize
)
{
	return xrtMemRangesOverlap(
		pData,
		iSize,
		pGroup,
		sizeof(*pGroup)
	);
}



/* 设置带稳定 WebSocket Group 域的结构化错误。 */
void __xrtWsGroupError(
	xerrkind Kind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsErrorSetDetail(
		Kind,
		"xrt.websocket.group",
		(int32)Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 为底层失败补充稳定的连接组操作边界。 */
void __xrtWsGroupWrap(
	xerrkind DefaultKind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xwsErrorWrapDetail(
		DefaultKind,
		"xrt.websocket.group",
		(int32)Code,
		sOperation,
		sMessage
	);
}



/* 以 Connection 指针值计算稳定的集合哈希。 */
static uint64 __xrtWsGroupHash(const void* pItem, ptr pData)
{
	(void)pData;
	return xrtHash64(pItem, sizeof(xwsconn*));
}



/* 比较两个 Connection 指针值。 */
static bool __xrtWsGroupEqual(
	const void* pLeft,
	const void* pRight,
	ptr pData
)
{
	(void)pData;
	return *(xwsconn* const*)pLeft == *(xwsconn* const*)pRight;
}



/* 为新成员取得一个独立 Connection 引用。 */
static bool __xrtWsGroupCopy(
	ptr pTarget,
	const void* pSource,
	ptr pData
)
{
	xwsconn* pConnection = *(xwsconn* const*)pSource;

	(void)pData;
	pConnection = xrtWsConnRef(pConnection);
	if ( pConnection == NULL ) {
		return false;
	}
	*(xwsconn**)pTarget = pConnection;
	return true;
}



/* 归还集合成员持有的 Connection 引用。 */
static void __xrtWsGroupDrop(ptr pItem, ptr pData)
{
	(void)pData;
	xrtWsConnDestroy(*(xwsconn**)pItem);
}



/* 初始化具有唯一 Connection 所有权语义的成员集合。 */
static bool __xrtWsGroupSetInit(xset* pConnections)
{
	memset(pConnections, 0, sizeof(*pConnections));
	if ( !xrtSetInit(pConnections, sizeof(xwsconn*)) ) {
		return false;
	}
	if ( !xrtSetSetKeyPolicy(
		pConnections,
		__xrtWsGroupHash,
		__xrtWsGroupEqual,
		NULL
	) || !xrtSetSetLifecycle(
		pConnections,
		__xrtWsGroupCopy,
		__xrtWsGroupDrop,
		NULL
	) ) {
		xrtSetUnit(pConnections);
		return false;
	}
	return true;
}



/* 验证快照头、计数乘法和完整连续分配区间。 */
static bool __xrtWsGroupSnapshotCheck(
	const xwsgroupsnapshot* pSnapshot,
	cstr sOperation,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xrtMemRangeValid(pSnapshot, sizeof(*pSnapshot)) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group snapshot range is invalid"
		);
		return false;
	}
	if ( pSnapshot->Count > ((SIZE_MAX - sizeof(*pSnapshot)) /
		sizeof(xwsconn*)) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			sOperation,
			"WebSocket group snapshot size overflows"
		);
		return false;
	}
	iSize = sizeof(*pSnapshot) +
		(pSnapshot->Count * sizeof(xwsconn*));
	if ( !xrtMemRangeValid(pSnapshot, iSize) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			sOperation,
			"WebSocket group snapshot storage is incomplete"
		);
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 在调用方持有组锁时检查成员是否存在。 */
static bool __xrtWsGroupHasLocked(
	const xwsgroup* pGroup,
	const xwsconn* pConnection
)
{
	xwsconn* pKey = (xwsconn*)pConnection;

	return xrtSetHas(&pGroup->Connections, &pKey);
}



/* 快照访问器按集合插入顺序增加 Connection 引用。 */
static bool __xrtWsGroupSnapshotAdd(const void* pItem, ptr pData)
{
	xwsgroupsnapshot* pSnapshot = (xwsgroupsnapshot*)pData;
	xwsconn* pConnection = *(xwsconn* const*)pItem;

	pConnection = xrtWsConnRef(pConnection);
	if ( pConnection == NULL ) {
		return false;
	}
	pSnapshot->Connections[pSnapshot->Count] = pConnection;
	pSnapshot->Count++;
	return true;
}



/* 创建连接组；Limit 为零表示不设置成员数量上限。 */
XRT_API xwsgroup* xrtWsGroupCreate(size_t iLimit)
{
	xwsgroup* pGroup = (xwsgroup*)xrtCalloc(1, sizeof(*pGroup));

	if ( pGroup == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.create",
			"WebSocket group allocation failed"
		);
		return NULL;
	}
	pGroup->References = 1;
	pGroup->Limit = iLimit;
	if ( !xrtMutexInit(&pGroup->Lock) ) {
		xrtFree(pGroup);
		return NULL;
	}
	if ( !__xrtWsGroupSetInit(&pGroup->Connections) ) {
		(void)xrtMutexUnit(&pGroup->Lock);
		xrtFree(pGroup);
		return NULL;
	}
	return pGroup;
}



/* 增加连接组引用并返回原指针。 */
XRT_API xwsgroup* xrtWsGroupRef(xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.ref"
	) ) {
		return NULL;
	}
	if ( xrtRefRetain(&pGroup->References) < 0 ) {
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.ref",
			"WebSocket group reference cannot be retained"
		);
		return NULL;
	}
	return pGroup;
}



/* 释放连接组引用；最后一个引用只释放成员，不关闭连接。 */
XRT_API void xrtWsGroupDestroy(xwsgroup* pGroup)
{
	if ( pGroup == NULL ) {
		return;
	}
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.destroy"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pGroup->References) != 0 ) {
		return;
	}
	xrtSetUnit(&pGroup->Connections);
	(void)xrtMutexUnit(&pGroup->Lock);
	xrtFree(pGroup);
}



/* 加入并持有 Connection；重复加入成功且不增加第二个引用。 */
XRT_API bool xrtWsGroupAdd(xwsgroup* pGroup, xwsconn* pConnection)
{
	bool bAdded;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.add"
	) ) {
		return false;
	}
	if ( !__xrtWsConnRangeValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.add",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	if ( __xrtWsGroupHasLocked(pGroup, pConnection) ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		return true;
	}
	if ( pGroup->Sealed ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		__xrtWsGroupError(
			XERR_STATE,
			XWS_GROUP_ERROR_STATE,
			"websocket-group.add",
			"WebSocket group is sealed"
		);
		return false;
	}
	if ( (pGroup->Limit != 0) &&
		(xrtSetCount(&pGroup->Connections) >= pGroup->Limit) ) {
		(void)xrtMutexUnlock(&pGroup->Lock);
		__xrtWsGroupError(
			XERR_AGAIN,
			XWS_GROUP_ERROR_CAPACITY,
			"websocket-group.add",
			"WebSocket group member limit is reached"
		);
		return false;
	}
	bAdded = xrtSetAdd(&pGroup->Connections, &pConnection);
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( !bAdded ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.add",
			"WebSocket group member allocation failed"
		);
	}
	return bAdded;
}



/* 移除并释放 Connection；成员不存在时正常返回 false。 */
XRT_API bool xrtWsGroupRemove(xwsgroup* pGroup, xwsconn* pConnection)
{
	xwsconn* pOwned = NULL;
	bool bRemoved;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.remove"
	) ) {
		return false;
	}
	if ( !__xrtWsConnRangeValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.remove",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	bRemoved = xrtSetTake(
		&pGroup->Connections,
		&pConnection,
		&pOwned
	);
	(void)xrtMutexUnlock(&pGroup->Lock);
	if ( bRemoved ) {
		xrtWsConnDestroy(pOwned);
	}
	return bRemoved;
}



/* 判断 Connection 是否属于当前组。 */
XRT_API bool xrtWsGroupHas(
	const xwsgroup* pGroup,
	const xwsconn* pConnection
)
{
	bool bPresent;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.has"
	) ) {
		return false;
	}
	if ( !__xrtWsConnRangeValid(pConnection) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.has",
			"WebSocket group or connection range is invalid"
		);
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return false;
	}
	bPresent = __xrtWsGroupHasLocked(pGroup, pConnection);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return bPresent;
}



/* 返回当前成员数量。 */
XRT_API size_t xrtWsGroupCount(const xwsgroup* pGroup)
{
	size_t iCount;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.count"
	) ) {
		return 0;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return 0;
	}
	iCount = xrtSetCount(&pGroup->Connections);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return iCount;
}



/* 返回创建时设置的硬上限；零表示没有显式上限。 */
XRT_API size_t xrtWsGroupLimit(const xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.limit"
	) ) {
		return 0;
	}
	return pGroup->Limit;
}



/* 永久阻止新成员加入；重复调用保持成功。 */
XRT_API bool xrtWsGroupSeal(xwsgroup* pGroup)
{
	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.seal"
	) ) {
		return false;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		return false;
	}
	pGroup->Sealed = true;
	(void)xrtMutexUnlock(&pGroup->Lock);
	return true;
}



/* 返回连接组是否已经封闭。 */
XRT_API bool xrtWsGroupSealed(const xwsgroup* pGroup)
{
	bool bSealed;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.sealed"
	) ) {
		return false;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return false;
	}
	bSealed = pGroup->Sealed;
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
	return bSealed;
}



/* 移除并释放全部成员，返回移除数量，但不重新开放已封闭的组。 */
XRT_API size_t xrtWsGroupClear(xwsgroup* pGroup)
{
	xset Empty;
	xset Previous;
	size_t iCount;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.clear"
	) ) {
		return 0;
	}
	if ( !__xrtWsGroupSetInit(&Empty) ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.clear",
			"WebSocket group replacement set initialization failed"
		);
		return 0;
	}
	if ( !xrtMutexLock(&pGroup->Lock) ) {
		xrtSetUnit(&Empty);
		return 0;
	}
	iCount = xrtSetCount(&pGroup->Connections);
	Previous = pGroup->Connections;
	pGroup->Connections = Empty;
	(void)xrtMutexUnlock(&pGroup->Lock);
	xrtSetUnit(&Previous);
	return iCount;
}



/* 成员增长时预留有限余量，避免高频加入导致连续精确重试。 */
static size_t __xrtWsGroupSnapshotGrow(
	const xwsgroup* pGroup,
	size_t iCount
)
{
	size_t iCapacity;
	size_t iExtra;
	size_t iMaximum = (SIZE_MAX - sizeof(xwsgroupsnapshot)) /
		sizeof(xwsconn*);

	if ( iCount >= iMaximum ) {
		return iCount;
	}
	iExtra = (iCount / 2u) + 8u;
	iCapacity = iExtra > (iMaximum - iCount) ?
		iMaximum : iCount + iExtra;
	if ( (pGroup->Limit != 0) &&
		(iCapacity > pGroup->Limit) ) {
		iCapacity = pGroup->Limit;
	}
	return iCapacity < iCount ? iCount : iCapacity;
}



/* 在锁外分配连续存储，并在成员增长时扩容重试。 */
XRT_API xwsgroupsnapshot* xrtWsGroupSnapshotCreate(
	const xwsgroup* pGroup
)
{
	xwsgroupsnapshot* pSnapshot;
	size_t iCapacity;
	size_t iCount;
	size_t iSize;
	size_t iVisited;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.snapshot"
	) ) {
		return NULL;
	}
	if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
		return NULL;
	}
	iCapacity = xrtSetCount(&pGroup->Connections);
	(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);

	for ( ;; ) {
		if ( iCapacity > ((SIZE_MAX - sizeof(*pSnapshot)) /
			sizeof(xwsconn*)) ) {
			__xrtWsGroupError(
				XERR_RANGE,
				XWS_GROUP_ERROR_RANGE,
				"websocket-group.snapshot",
				"WebSocket group snapshot size overflows"
			);
			return NULL;
		}
		iSize = sizeof(*pSnapshot) +
			(iCapacity * sizeof(xwsconn*));
		pSnapshot = (xwsgroupsnapshot*)xrtCalloc(1, iSize);
		if ( pSnapshot == NULL ) {
			__xrtWsGroupWrap(
				XERR_MEMORY,
				XWS_GROUP_ERROR_MEMORY,
				"websocket-group.snapshot",
				"WebSocket group snapshot allocation failed"
			);
			return NULL;
		}
		if ( !xrtMutexLock((xmutex*)&pGroup->Lock) ) {
			xrtFree(pSnapshot);
			return NULL;
		}
		iCount = xrtSetCount(&pGroup->Connections);
		if ( iCount > iCapacity ) {
			(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
			xrtFree(pSnapshot);
			iCapacity = __xrtWsGroupSnapshotGrow(
				pGroup,
				iCount
			);
			continue;
		}
		iVisited = xrtSetVisit(
			(xset*)&pGroup->Connections,
			__xrtWsGroupSnapshotAdd,
			pSnapshot
		);
		(void)xrtMutexUnlock((xmutex*)&pGroup->Lock);
		if ( iVisited != iCount ) {
			xrtWsGroupSnapshotDestroy(pSnapshot);
			__xrtWsGroupWrap(
				XERR_STATE,
				XWS_GROUP_ERROR_STATE,
				"websocket-group.snapshot",
				"WebSocket group snapshot could not retain every member"
			);
			return NULL;
		}
		return pSnapshot;
	}
}



/* 返回快照成员数量。 */
XRT_API size_t xrtWsGroupSnapshotCount(
	const xwsgroupsnapshot* pSnapshot
)
{
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-count",
		NULL
	) ) {
		return 0;
	}
	return pSnapshot->Count;
}



/* 借用指定快照成员；借用期不超过快照生命周期。 */
XRT_API xwsconn* xrtWsGroupSnapshotGet(
	const xwsgroupsnapshot* pSnapshot,
	size_t iIndex
)
{
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-get",
		NULL
	) ) {
		return NULL;
	}
	if ( iIndex >= pSnapshot->Count ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.snapshot-get",
			"WebSocket group snapshot index is out of range"
		);
		return NULL;
	}
	return pSnapshot->Connections[iIndex];
}



/* 释放快照和它持有的全部 Connection 引用。 */
XRT_API void xrtWsGroupSnapshotDestroy(xwsgroupsnapshot* pSnapshot)
{
	if ( pSnapshot == NULL ) {
		return;
	}
	if ( !__xrtWsGroupSnapshotCheck(
		pSnapshot,
		"websocket-group.snapshot-destroy",
		NULL
	) ) {
		return;
	}
	for ( size_t i = 0; i < pSnapshot->Count; i++ ) {
		xrtWsConnDestroy(pSnapshot->Connections[i]);
	}
	xrtFree(pSnapshot);
}

#endif
