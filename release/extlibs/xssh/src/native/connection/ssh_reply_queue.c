#include <xrt/ssh_reply_queue.h>



#if defined(XSSH_FEATURE_REPLY_QUEUE)

/* 计算 token 存储字节数并拒绝乘法溢出。 */
static bool xsshReplyStorageSize(size_t iCapacity, size_t* pSize)
{
	if ( (pSize == NULL) ||
		(iCapacity > (SIZE_MAX / sizeof(uint64))) ) {
		return false;
	}
	*pSize = iCapacity * sizeof(uint64);
	return true;
}



/* 校验 ring 状态与调用方存储范围。 */
static bool xsshReplyQueueValid(const xsshreplyqueue* pQueue)
{
	size_t iStorageSize;

	if ( (pQueue == NULL) || !pQueue->Initialized ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		!xrtMemRangeValid(pQueue->Tokens, iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pQueue,
			sizeof(*pQueue)
		) || (pQueue->Count > pQueue->Capacity) ) {
		return false;
	}
	return pQueue->Capacity == 0u ? pQueue->Head == 0u :
		pQueue->Head < pQueue->Capacity;
}



/* 计算不发生 size_t 回绕的 ring 下标。 */
static size_t xsshReplyIndex(
	const xsshreplyqueue* pQueue,
	size_t iOffset
)
{
	size_t iUntilEnd = pQueue->Capacity - pQueue->Head;

	return iOffset >= iUntilEnd ?
		iOffset - iUntilEnd : pQueue->Head + iOffset;
}



/* 初始化调用方提供的 token ring。 */
bool xrtSshReplyQueueInit(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
)
{
	xsshreplyqueue Queue;
	size_t iStorageSize;

	if ( (pQueue == NULL) ||
		!xsshReplyStorageSize(iCapacity, &iStorageSize) ||
		!xrtMemRangeValid(pTokens, iStorageSize) ||
		xrtMemRangesOverlap(
			pTokens,
			iStorageSize,
			pQueue,
			sizeof(*pQueue)
		) ) {
		return false;
	}
	Queue.Tokens = pTokens;
	Queue.Capacity = iCapacity;
	Queue.Head = 0u;
	Queue.Count = 0u;
	Queue.Initialized = true;
	*pQueue = Queue;
	return true;
}



/* 返回等待回复数量。 */
size_t xrtSshReplyQueueCount(const xsshreplyqueue* pQueue)
{
	return xsshReplyQueueValid(pQueue) ? pQueue->Count : 0u;
}



/* 追加已经可靠发送的 want-reply 请求 token。 */
xsshcode xrtSshReplyQueuePush(
	xsshreplyqueue* pQueue,
	uint64 iToken
)
{
	size_t iTail;

	if ( !xsshReplyQueueValid(pQueue) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == pQueue->Capacity ) {
		return XSSH_ERROR_SPACE;
	}
	iTail = xsshReplyIndex(pQueue, pQueue->Count);
	pQueue->Tokens[iTail] = iToken;
	++pQueue->Count;
	return XSSH_OK;
}



/* 查看但不消费队首 token。 */
xsshcode xrtSshReplyQueueFront(
	const xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	uint64 iToken;
	size_t iStorageSize;

	if ( !xsshReplyQueueValid(pQueue) || (pToken == NULL) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pToken,
			sizeof(*pToken)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iToken = pQueue->Tokens[pQueue->Head];
	*pToken = iToken;
	return XSSH_OK;
}



/* 按线路顺序消费队首 token。 */
xsshcode xrtSshReplyQueuePop(
	xsshreplyqueue* pQueue,
	uint64* pToken
)
{
	uint64 iToken;
	size_t iStorageSize;

	if ( !xsshReplyQueueValid(pQueue) || (pToken == NULL) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iStorageSize) ||
		xrtMemRangesOverlap(
			pQueue->Tokens,
			iStorageSize,
			pToken,
			sizeof(*pToken)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pQueue->Count == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	iToken = pQueue->Tokens[pQueue->Head];
	if ( pQueue->Head == (pQueue->Capacity - 1u) ) {
		pQueue->Head = 0u;
	} else {
		++pQueue->Head;
	}
	--pQueue->Count;
	*pToken = iToken;
	return XSSH_OK;
}



/* 将 ring 按逻辑顺序迁移到新的不重叠存储。 */
xsshcode xrtSshReplyQueueRebind(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
)
{
	size_t iOldSize;
	size_t iNewSize;
	size_t i;

	if ( !xsshReplyQueueValid(pQueue) ||
		!xsshReplyStorageSize(pQueue->Capacity, &iOldSize) ||
		!xsshReplyStorageSize(iCapacity, &iNewSize) ||
		(iCapacity < pQueue->Count) ||
		!xrtMemRangeValid(pTokens, iNewSize) ||
		xrtMemRangesOverlap(
			pTokens,
			iNewSize,
			pQueue,
			sizeof(*pQueue)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pTokens == pQueue->Tokens) && (iCapacity == pQueue->Capacity) ) {
		return XSSH_OK;
	}
	if ( xrtMemRangesOverlap(
		pQueue->Tokens,
		iOldSize,
		pTokens,
		iNewSize
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < pQueue->Count; ++i ) {
		pTokens[i] = pQueue->Tokens[xsshReplyIndex(pQueue, i)];
	}
	pQueue->Tokens = pTokens;
	pQueue->Capacity = iCapacity;
	pQueue->Head = 0u;
	return XSSH_OK;
}

#endif
