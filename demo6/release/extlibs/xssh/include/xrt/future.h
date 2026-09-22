#ifndef XRT_FUTURE_H
#define XRT_FUTURE_H

#include <xrt/cancel.h>
#include <xrt/error.h>



#if defined(XRT_FEATURE_FUTURE) && !defined(XRT_FEATURE_CANCEL)
	#error "XRT_FEATURE_FUTURE requires XRT_FEATURE_CANCEL"
#endif

#if defined(XRT_FEATURE_FUTURE_COROUTINE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_COROUTINE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_FUTURE_COROUTINE) && !defined(XRT_FEATURE_COROUTINE_SCHEDULER)
	#error "XRT_FEATURE_FUTURE_COROUTINE requires XRT_FEATURE_COROUTINE_SCHEDULER"
#endif

#if defined(XRT_FEATURE_FUTURE_COMBINE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_COMBINE requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_FUTURE_CONTINUE) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_FUTURE_CONTINUE requires XRT_FEATURE_FUTURE"
#endif



#if defined(XRT_FEATURE_FUTURE)

/* Future 是只读共享结果，Promise 是唯一终态写入端。 */
typedef struct xfuture xfuture;
typedef struct xpromise xpromise;



/* Future 终态明确区分成功、失败、协作取消和生产端关闭。 */
typedef enum xfuturestate {
	XFUTURE_PENDING = 0,
	XFUTURE_RESOLVED = 1,
	XFUTURE_FAILED = 2,
	XFUTURE_CANCELLED = 3,
	XFUTURE_CLOSED = 4
} xfuturestate;



/* Future 结果只借用值和错误，其生命周期由 Future 引用保护。 */
typedef struct xfutureresult {
	xfuturestate State;
	ptr Value;
	const xerror* Error;
} xfutureresult;



/* Future Watch 使用调用方存储提供无分配终态通知。 */
#define XRT_FUTURE_WATCH_STORAGE_SIZE 64u



/* Watch 回调在线程安全的 Future 完成路径中执行，不得重入同一个 Watch。 */
typedef void (*xfuturewatchproc)(ptr pData);



/* Watch 释放过程在线性化完成通知或成功摘除后执行一次。 */
typedef void (*xfuturewatchreleaseproc)(ptr pData);



/* Watch 的内部链表和并发状态保持不透明。 */
typedef union xfuturewatch {
	uint64 Alignment;
	uint8 Storage[XRT_FUTURE_WATCH_STORAGE_SIZE];
} xfuturewatch;



/* 注册结果区分错误、Future 已完成和成功进入等待链。 */
typedef enum xfuturewatchresult {
	XFUTURE_WATCH_ERROR = -1,
	XFUTURE_WATCH_READY = 0,
	XFUTURE_WATCH_PENDING = 1
} xfuturewatchresult;



/* 成功值析构过程接收创建者提供的值和上下文。 */
typedef void (*xfuturefreeproc)(ptr pValue, ptr pData);



XRT_EXTERN_C_BEGIN



/* 创建一对 Future/Promise；父取消令牌为空时使用独立取消源。 */
XRT_API xpromise* xrtPromiseCreate(xfuture** ppFuture, xcancel* pParentCancel);



/* 增加 Promise 生产端引用并返回原指针。 */
XRT_API xpromise* xrtPromiseRef(xpromise* pPromise);



/* 释放生产端引用；最后一个未完成生产端会关闭 Future 并请求取消。 */
XRT_API void xrtPromiseDestroy(xpromise* pPromise);



/* 增加 Future 消费端引用并返回原指针。 */
XRT_API xfuture* xrtFutureRef(xfuture* pFuture);



/* 释放 Future 消费端引用；空指针视为空操作。 */
XRT_API void xrtFutureDestroy(xfuture* pFuture);



/* 返回 Future 状态快照；参数无效时返回 CLOSED 并设置错误。 */
XRT_API xfuturestate xrtFutureState(const xfuture* pFuture);



/* 判断 Future 是否已经进入任一不可变终态。 */
XRT_API bool xrtFutureDone(const xfuture* pFuture);



/* 复制借用结果；尚未完成时返回 false 并设置 AGAIN。 */
XRT_API bool xrtFutureResult(const xfuture* pFuture, xfutureresult* pResult);



/* 返回成功值；非成功终态会把对应错误设置到当前执行上下文。 */
XRT_API ptr xrtFutureValue(const xfuture* pFuture);



/* 返回失败终态借用的结构化错误，其他状态返回空指针。 */
XRT_API const xerror* xrtFutureError(const xfuture* pFuture);



/* 请求生产过程协作取消；请求本身不伪造 Future 终态。 */
XRT_API bool xrtFutureCancel(xfuture* pFuture);



/* 返回增加引用后的取消令牌，调用方使用完毕后必须释放。 */
XRT_API xcancel* xrtFutureCancelToken(const xfuture* pFuture);



/* 返回增加引用后的生产端取消令牌。 */
XRT_API xcancel* xrtPromiseCancelToken(const xpromise* pPromise);



/* 初始化一个尚未注册的无分配 Future Watch。 */
XRT_API bool xrtFutureWatchInit(
	xfuturewatch* pWatch,
	xfuturewatchproc pNotify,
	xfuturewatchreleaseproc pRelease,
	ptr pData
);



/* Future 未完成时注册 Watch；READY 时 Watch 未被接管且不执行 Release。 */
XRT_API xfuturewatchresult xrtFutureWatchAdd(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 尝试摘除尚未开始通知的 Watch；成功时同步执行 Release。 */
XRT_API bool xrtFutureWatchDetach(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 摘除 Watch 并等待已经开始的通知结束；禁止从自身通知中调用。 */
XRT_API void xrtFutureWatchRemove(
	xfuture* pFuture,
	xfuturewatch* pWatch
);



/* 等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWait(xfuture* pFuture);



/* 在相对微秒数内等待 Future 进入任一终态。 */
XRT_API xwaitresult xrtFutureWaitFor(xfuture* pFuture, uint64 iTimeout);



/* 等待 Future 到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtFutureWaitUntil(xfuture* pFuture, xdeadline iDeadline);



/* 等待首个线性化事件；取消先取得等待锁后不会被迟到终态覆盖。 */
XRT_API xwaitresult xrtFutureWaitUntilCancel(
	xfuture* pFuture,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 以借用方式完成成功结果，值的生命周期由调用方保证。 */
XRT_API bool xrtPromiseResolve(xpromise* pPromise, ptr pValue);



/* 转移成功值所有权；完成失败时所有权仍归调用方。 */
XRT_API bool xrtPromiseResolveOwned(
	xpromise* pPromise,
	ptr pValue,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 以增加引用方式完成失败结果。 */
XRT_API bool xrtPromiseReject(xpromise* pPromise, const xerror* pError);



/* 把已进入终态的源 Future 结果安全透传到 Promise。 */
XRT_API bool xrtPromiseForward(xpromise* pPromise, xfuture* pSource);



/* 完成取消终态；令牌请求通知结束后才向等待者发布取消终态。 */
XRT_API bool xrtPromiseCancel(xpromise* pPromise);



/* 请求生产过程停止，并在令牌通知结束后发布关闭终态。 */
XRT_API bool xrtPromiseClose(xpromise* pPromise);



/* 判断 Promise 对应的 Future 是否已经完成。 */
XRT_API bool xrtPromiseDone(const xpromise* pPromise);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_COMBINE)

/* Any 与 Race 的结果借用胜出源 Future；组合 Future 负责保留该引用。 */
typedef struct xfuturepick {
	size_t Index;
	xfuture* Future;
} xfuturepick;



/* All 的结果按输入顺序借用全部源 Future；组合 Future 负责保留这些引用。 */
typedef struct xfutureall {
	size_t Count;
	xfuture* const* Futures;
} xfutureall;



XRT_EXTERN_C_BEGIN



/* 在任一源进入终态后，以 xfuturepick 成功完成；不改变其余源。 */
XRT_API xfuture* xrtFutureAny(xfuture* const* pFutures, size_t iCount);



/* 在全部源进入终态后，以保序的 xfutureall 成功完成；空集合立即完成。 */
XRT_API xfuture* xrtFutureAll(xfuture* const* pFutures, size_t iCount);



/* 在任一源进入终态后完成，并向其余未完成源发出协作取消请求。 */
XRT_API xfuture* xrtFutureRace(xfuture* const* pFutures, size_t iCount);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_CONTINUE)

/* 延续过程借用源结果和输出 Promise；保留 Promise 时必须先增加引用。 */
typedef void (*xfuturecontinueproc)(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
);



/* Finally 过程只观察源结果，输出 Future 自动安全透传源终态。 */
typedef void (*xfuturefinallyproc)(const xfutureresult* pInput, ptr pData);



XRT_EXTERN_C_BEGIN



/* 对源的任意终态执行延续过程；过程负责完成或保留输出 Promise。 */
XRT_API xfuture* xrtFutureContinue(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 执行任意终态延续，并在执行、跳过或取消后释放受理的数据。 */
XRT_API xfuture* xrtFutureContinueOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/*
	对独占源的任意终态执行延续；取消输出时同时请求取消源。
	该入口只适用于调用方拥有完整生产链、不与其他消费者共享源的组合层。
*/
XRT_API xfuture* xrtFutureContinueOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 仅在源成功时执行延续；其他终态自动透传。 */
XRT_API xfuture* xrtFutureThen(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 仅在源成功时执行延续，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureThenOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/*
	仅在独占源成功时执行延续；取消输出时同时请求取消源。
	源的失败、取消和关闭仍按 Then 契约自动透传。
*/
XRT_API xfuture* xrtFutureThenOwnedCancelSource(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 仅在源失败时执行延续；成功、取消和关闭终态自动透传。 */
XRT_API xfuture* xrtFutureCatch(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData
);



/* 仅在源失败时执行延续，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureCatchOwned(
	xfuture* pSource,
	xfuturecontinueproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



/* 观察源的任意终态，再把原结果安全透传到输出 Future。 */
XRT_API xfuture* xrtFutureFinally(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData
);



/* 观察源的任意终态、透传结果，并负责释放受理的数据。 */
XRT_API xfuture* xrtFutureFinallyOwned(
	xfuture* pSource,
	xfuturefinallyproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy,
	ptr pDestroyData
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_FUTURE_COROUTINE)

XRT_EXTERN_C_BEGIN



/* 挂起当前调度协程，直到 Future 进入终态或协程被取消。 */
XRT_API xwaitresult xrtFutureAwait(xfuture* pFuture);



/* 在相对微秒数内挂起当前调度协程等待 Future。 */
XRT_API xwaitresult xrtFutureAwaitFor(xfuture* pFuture, uint64 iTimeout);



/* 挂起当前调度协程等待 Future 到指定截止时间。 */
XRT_API xwaitresult xrtFutureAwaitUntil(xfuture* pFuture, xdeadline iDeadline);



XRT_EXTERN_C_END

#endif

#endif
