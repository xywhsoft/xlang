#ifndef XRT_SSH_REPLY_QUEUE_H
#define XRT_SSH_REPLY_QUEUE_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_REPLY_QUEUE) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_REPLY_QUEUE requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_REPLY_QUEUE)

/* Reply FIFO 使用调用方 token 存储，不拥有 future、任务或内存分配。 */
typedef struct xsshreplyqueue {
	uint64* Tokens;
	size_t Capacity;
	size_t Head;
	size_t Count;
	bool Initialized;
} xsshreplyqueue;



XRT_EXTERN_C_BEGIN



/* 初始化调用方存储支持的 reply FIFO；零容量允许 Tokens 为 NULL。 */
XRT_API bool xrtSshReplyQueueInit(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
);



/* 返回当前等待回复的请求数量；无效状态返回零。 */
XRT_API size_t xrtSshReplyQueueCount(const xsshreplyqueue* pQueue);



/* 在 want-reply 请求可靠排队后追加调用方 token。 */
XRT_API xsshcode xrtSshReplyQueuePush(
	xsshreplyqueue* pQueue,
	uint64 iToken
);



/* 查看队首 token，不消费对应回复位置。 */
XRT_API xsshcode xrtSshReplyQueueFront(
	const xsshreplyqueue* pQueue,
	uint64* pToken
);



/* 收到 success/failure 时按协议顺序消费队首 token。 */
XRT_API xsshcode xrtSshReplyQueuePop(
	xsshreplyqueue* pQueue,
	uint64* pToken
);



/* 将未完成 token 按顺序迁移到更大的不重叠调用方存储。 */
XRT_API xsshcode xrtSshReplyQueueRebind(
	xsshreplyqueue* pQueue,
	uint64* pTokens,
	size_t iCapacity
);



XRT_EXTERN_C_END

#endif

#endif
