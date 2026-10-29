#include "xl_clock.h"
#ifndef XRT_WEBSOCKET_GROUP_H
#define XRT_WEBSOCKET_GROUP_H

#include "../xws/features.h"
#include "websocket_runtime.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_SET) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT WebSocket group requires connection, set and mutex"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_GROUP) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "XRT WebSocket group Future requires group, connection Future, reference Future and Future"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_GROUP)

/* 连接组错误用于区分参数、内存、状态、容量和索引边界。 */
typedef enum xwsgrouperror {
	XWS_GROUP_ERROR_ARGUMENT = 1,
	XWS_GROUP_ERROR_MEMORY,
	XWS_GROUP_ERROR_STATE,
	XWS_GROUP_ERROR_CAPACITY,
	XWS_GROUP_ERROR_RANGE
} xwsgrouperror;



/* 连接组并发持有不重复的 Connection 引用。 */
typedef struct xwsgroup xwsgroup;



/* 快照独立持有创建时的全部 Connection 引用。 */
typedef struct xwsgroupsnapshot xwsgroupsnapshot;

#endif



#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 批量操作槽位区分同步拒绝、等待和五种 Future 终态。 */
typedef enum xwsgroupopstate {
	XWS_GROUP_OP_REJECTED = 0,
	XWS_GROUP_OP_PENDING,
	XWS_GROUP_OP_RESOLVED,
	XWS_GROUP_OP_FAILED,
	XWS_GROUP_OP_CANCELLED,
	XWS_GROUP_OP_CLOSED
} xwsgroupopstate;



/* 槽位结果借用操作对象持有的 Connection 和错误。 */
typedef struct xwsgroupopresult {
	xwsgroupopstate State;
	xwsconn* Connection;
	const xerror* Error;
} xwsgroupopresult;



/* 批量操作保存稳定成员快照、逐成员 Future 和同步拒绝原因。 */
typedef struct xwsgroupop xwsgroupop;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XWS_FEATURE_WEBSOCKET_GROUP)

/* 创建连接组；Limit 为零表示不设置成员数量上限。 */
XRT_API xwsgroup* xrtWsGroupCreate(size_t iLimit);



/* 增加连接组引用并返回原指针。 */
XRT_API xwsgroup* xrtWsGroupRef(xwsgroup* pGroup);



/* 释放连接组引用；最后一个引用只释放成员，不关闭连接。 */
XRT_API void xrtWsGroupDestroy(xwsgroup* pGroup);



/* 加入并持有 Connection；重复加入成功且不增加第二个引用。 */
XRT_API bool xrtWsGroupAdd(xwsgroup* pGroup, xwsconn* pConnection);



/* 移除并释放 Connection；成员不存在时正常返回 false。 */
XRT_API bool xrtWsGroupRemove(xwsgroup* pGroup, xwsconn* pConnection);



/* 判断 Connection 是否属于当前组。 */
XRT_API bool xrtWsGroupHas(const xwsgroup* pGroup, const xwsconn* pConnection);



/* 返回当前成员数量。 */
XRT_API size_t xrtWsGroupCount(const xwsgroup* pGroup);



/* 返回创建时设置的硬上限；零表示没有显式上限。 */
XRT_API size_t xrtWsGroupLimit(const xwsgroup* pGroup);



/* 永久阻止新成员加入；重复调用保持成功。 */
XRT_API bool xrtWsGroupSeal(xwsgroup* pGroup);



/* 返回连接组是否已经封闭。 */
XRT_API bool xrtWsGroupSealed(const xwsgroup* pGroup);



/* 原子移走全部成员并在组锁外释放，返回数量且不重新开放已封闭的组。 */
XRT_API size_t xrtWsGroupClear(xwsgroup* pGroup);



/* 在组锁外分配并创建保序成员快照；空组也返回有效空快照。 */
XRT_API xwsgroupsnapshot* xrtWsGroupSnapshotCreate(const xwsgroup* pGroup);



/* 返回快照成员数量。 */
XRT_API size_t xrtWsGroupSnapshotCount(const xwsgroupsnapshot* pSnapshot);



/* 借用指定快照成员；借用期不超过快照生命周期。 */
XRT_API xwsconn* xrtWsGroupSnapshotGet(
	const xwsgroupsnapshot* pSnapshot,
	size_t iIndex
);



/* 释放快照和它持有的全部 Connection 引用。 */
XRT_API void xrtWsGroupSnapshotDestroy(xwsgroupsnapshot* pSnapshot);

#endif



XRT_EXTERN_C_END



#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)

XRT_EXTERN_C_BEGIN



/* 对调用时的稳定成员快照异步提交一条 Text 或 Binary 消息；负载只复制一次。 */
XRT_API xwsgroupop* xrtWsGroupSendAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交 UTF-8 Text。 */
XRT_API xwsgroupop* xrtWsGroupTextAsync(
	xwsgroup* pGroup,
	xstrview Text
);



/* 对调用时的稳定成员快照异步提交 Binary。 */
XRT_API xwsgroupop* xrtWsGroupBinaryAsync(
	xwsgroup* pGroup,
	xbytesview Data
);



/*
	异步提交共享所有权消息；仅成功返回操作对象时接管 Ref。
	每个已接纳成员持有独立共享引用，最后一个引用恰好调用一次 Release。
	Ref 结构允许未对齐；结构与负载必须是完整范围且不能覆盖 Group。
*/
XRT_API xwsgroupop* xrtWsGroupSendRefAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 异步提交共享所有权 UTF-8 Text。 */
XRT_API xwsgroupop* xrtWsGroupTextRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
);



/* 异步提交共享所有权 Binary。 */
XRT_API xwsgroupop* xrtWsGroupBinaryRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
);



/* 对调用时的稳定成员快照异步提交 Ping。 */
XRT_API xwsgroupop* xrtWsGroupPingAsync(
	xwsgroup* pGroup,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交 Pong。 */
XRT_API xwsgroupop* xrtWsGroupPongAsync(
	xwsgroup* pGroup,
	xbytesview Payload
);



/* 对调用时的稳定成员快照异步提交唯一 Close。 */
XRT_API xwsgroupop* xrtWsGroupCloseAsync(
	xwsgroup* pGroup,
	uint16 iCode,
	xstrview Reason
);



/* 对调用时的稳定成员快照异步等待同一种 Connection 条件。 */
XRT_API xwsgroupop* xrtWsGroupWaitAsync(
	xwsgroup* pGroup,
	xwsconnwait Wait
);



/* 增加批量操作引用并返回原指针。 */
XRT_API xwsgroupop* xrtWsGroupOpRef(xwsgroupop* pOperation);



/* 释放批量操作引用；已提交的 Connection 操作不会因此取消。 */
XRT_API void xrtWsGroupOpDestroy(xwsgroupop* pOperation);



/* 返回稳定成员槽位总数。 */
XRT_API size_t xrtWsGroupOpCount(const xwsgroupop* pOperation);



/* 返回成功创建逐成员 Future 的槽位数。 */
XRT_API size_t xrtWsGroupOpAccepted(const xwsgroupop* pOperation);



/* 返回提交阶段同步拒绝的槽位数。 */
XRT_API size_t xrtWsGroupOpRejected(const xwsgroupop* pOperation);



/* 以 O(1) 并发快照返回同步拒绝或已经进入 Future 终态的槽位数。 */
XRT_API size_t xrtWsGroupOpDoneCount(const xwsgroupop* pOperation);



/* 读取指定槽位；输出允许未对齐，但必须完整且不能覆盖 Operation。 */
XRT_API bool xrtWsGroupOpResult(
	const xwsgroupop* pOperation,
	size_t iIndex,
	xwsgroupopresult* pResult
);



/* 返回增加引用后的逐成员 Future；同步拒绝槽位返回空指针。 */
XRT_API xfuture* xrtWsGroupOpItemFutureRef(
	const xwsgroupop* pOperation,
	size_t iIndex
);



/*
	返回增加引用后的完成 Future；正常时在全部已接纳操作进入终态后成功完成。
	取消该 Future 会立即取消聚合等待，并向仍未结束的逐成员 Future 传播取消请求。
*/
XRT_API xfuture* xrtWsGroupOpFutureRef(const xwsgroupop* pOperation);



/* 向全部已接纳且尚未结束的逐成员 Future 请求协作取消，并返回成功请求数。 */
XRT_API size_t xrtWsGroupOpCancel(xwsgroupop* pOperation);



/* 等待全部已接纳操作进入终态。 */
XRT_API xwaitresult xrtWsGroupOpWait(xwsgroupop* pOperation);



/* 在相对毫秒数内等待全部已接纳操作进入终态。 */
XRT_API xwaitresult xrtWsGroupOpWaitFor(
	xwsgroupop* pOperation,
	uint64 iTimeout
);



/* 等待全部已接纳操作到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	XL6_DeadlineV1 iDeadline
);



/* 等待批量操作、截止时间或调用方取消令牌中的首个事件。 */
XRT_API xwaitresult xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	XL6_DeadlineV1 iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif
