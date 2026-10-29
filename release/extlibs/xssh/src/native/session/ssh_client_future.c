#include <string.h>

#include <xrt/spin.h>

#include "ssh_client_future_internal.h"



#if defined(XSSH_FEATURE_CLIENT_FUTURE)

/* 等待节点类型保持公共 API 参数精确，不使用带无关字段的万能入口。 */
typedef enum xsshclientfuturekind {
	XSSH_CLIENT_FUTURE_WAIT_CLIENT = 0,
	XSSH_CLIENT_FUTURE_WAIT_CHANNEL = 1,
	XSSH_CLIENT_FUTURE_WAIT_READ = 2,
	XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY = 3,
	XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY = 4
} xsshclientfuturekind;



/* 内部完成状态直接映射到 XRT Future 的四种终态。 */
typedef enum xsshclientfuturecompletion {
	XSSH_CLIENT_FUTURE_PENDING = 0,
	XSSH_CLIENT_FUTURE_RESOLVE = 1,
	XSSH_CLIENT_FUTURE_REJECT = 2,
	XSSH_CLIENT_FUTURE_CANCEL = 3,
	XSSH_CLIENT_FUTURE_CLOSED = 4
} xsshclientfuturecompletion;



typedef struct xsshclientfuturestate xsshclientfuturestate;
typedef struct xsshclientfuturewaiter xsshclientfuturewaiter;



/* 每个 Future 只占一个链表节点、Promise 和取消监听。 */
struct xsshclientfuturewaiter {
	xsshclientfuturewaiter* Next;
	xsshclientfuturestate* State;
	xpromise* Promise;
	xcancelwatch* CancelWatch;
	xsshchannel* Channel;
	uint64 ReplyToken;
	uint32 ChannelLocal;
	xsshclientfuturekind Kind;
	xsshclientwait ClientWait;
	xsshclientchannelwait ChannelWait;
	xsshchanneliostream Stream;
	bool Linked;
};



/* 管理器按客户端懒创建；引用保护 Close 回调中的同步 Future continuation。 */
struct xsshclientfuturestate {
	xspinlock Lock;
	xatomic32 References;
	xsshclientfuturewaiter* Head;
	xsshclientfuturewaiter* Tail;
	bool Closed;
};



/* 增加管理器内部引用。 */
static void xsshClientFutureStateRef(xsshclientfuturestate* pState)
{
	(void)xrtAtomic32FetchAdd(
		&pState->References,
		1u,
		XMEMORY_RELAXED
	);
}



/* 释放最后一个管理器引用及其关闭错误。 */
static void xsshClientFutureStateDestroy(xsshclientfuturestate* pState)
{
	if ( xrtAtomic32FetchAdd(
		&pState->References,
		UINT32_MAX,
		XMEMORY_ACQ_REL
	) != 1u ) {
		return;
	}
	(void)xrtSpinUnit(&pState->Lock);
	xrtFree(pState);
}



/* 在客户端 Worker 上按需建立唯一等待管理器。 */
static xsshclientfuturestate* xsshClientFutureState(
	xsshclient* pClient
)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;

	if ( pState != NULL ) {
		return pState;
	}
	pState = (xsshclientfuturestate*)xrtCalloc(1u, sizeof(*pState));
	if ( pState == NULL ) {
		return NULL;
	}
	if ( !xrtSpinInit(&pState->Lock) ) {
		xrtFree(pState);
		return NULL;
	}
	xrtAtomic32Init(&pState->References, 1u);
	pClient->FutureState = pState;
	return pState;
}



/* 从等待链中摘除一个已知节点。 */
static bool xsshClientFutureRemove(
	xsshclientfuturestate* pState,
	xsshclientfuturewaiter* pWaiter
)
{
	xsshclientfuturewaiter** ppCurrent = &pState->Head;
	xsshclientfuturewaiter* pPrevious = NULL;

	while ( (*ppCurrent != NULL) && (*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pState->Tail == pWaiter ) {
		pState->Tail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	return true;
}



/* 把一个摘除节点发布到唯一 Future 终态并释放生产资源。 */
static void xsshClientFutureFinish(
	xsshclientfuturewaiter* pWaiter,
	xsshclientfuturecompletion Completion,
	const xerror* pError
)
{
	xpromise* pPromise = pWaiter->Promise;
	xsshclientfuturestate* pState = pWaiter->State;

	pWaiter->Promise = NULL;
	xrtCancelUnwatch(pWaiter->CancelWatch);
	pWaiter->CancelWatch = NULL;
	if ( Completion == XSSH_CLIENT_FUTURE_RESOLVE ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( (Completion == XSSH_CLIENT_FUTURE_REJECT) &&
		(pError != NULL) ) {
		(void)xrtPromiseReject(pPromise, pError);
	} else if ( Completion == XSSH_CLIENT_FUTURE_CANCEL ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	xsshClientFutureStateDestroy(pState);
	xrtFree(pWaiter);
}



/* Future 取消只摘除本次等待，不改变 SSH channel 或连接。 */
static void xsshClientFutureCancel(ptr pData)
{
	xsshclientfuturewaiter* pWaiter =
		(xsshclientfuturewaiter*)pData;
	xsshclientfuturestate* pState = pWaiter->State;
	bool bRemoved;

	(void)xrtSpinLock(&pState->Lock);
	bRemoved = pWaiter->Linked &&
		xsshClientFutureRemove(pState, pWaiter);
	(void)xrtSpinUnlock(&pState->Lock);
	if ( bRemoved ) {
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CANCEL,
			NULL
		);
	}
}



/* 创建一个可立即完成或进入管理器链表的 Future 节点。 */
static xsshclientfuturewaiter* xsshClientFutureCreate(
	xsshclient* pClient,
	xfuture** ppFuture
)
{
	xsshclientfuturestate* pState;
	xsshclientfuturewaiter* pWaiter;
	xcancel* pCancel;
	xerror* pError;

	*ppFuture = NULL;
	if ( !xrtSshClientIsCurrent(pClient) ) {
		xrtSetErrorInfo(
			XERR_STATE,
			"xrt.ssh.client.future",
			(int32)XSSH_ERROR_STATE,
			"SSH client Future must be created on its network Worker"
		);
		return NULL;
	}
	pState = xsshClientFutureState(pClient);
	if ( pState == NULL ) {
		return NULL;
	}
	pWaiter = (xsshclientfuturewaiter*)xrtCalloc(
		1u,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(ppFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->State = pState;
	xsshClientFutureStateRef(pState);
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->CancelWatch = xrtCancelWatch(
			pCancel,
			xsshClientFutureCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->CancelWatch != NULL ) {
		return pWaiter;
	}
	pError = xrtTakeError();
	xrtFutureDestroy(*ppFuture);
	*ppFuture = NULL;
	xrtPromiseDestroy(pWaiter->Promise);
	xsshClientFutureStateDestroy(pState);
	xrtFree(pWaiter);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return NULL;
}



/* 把节点链接到管理器尾部，保持同条件 waiter 的 FIFO 完成顺序。 */
static void xsshClientFutureLink(
	xsshclientfuturestate* pState,
	xsshclientfuturewaiter* pWaiter
)
{
	pWaiter->Linked = true;
	if ( pState->Tail != NULL ) {
		pState->Tail->Next = pWaiter;
	} else {
		pState->Head = pWaiter;
	}
	pState->Tail = pWaiter;
}



/* 返回客户端水平条件的当前结果。 */
static xsshclientfuturecompletion xsshClientFutureClientResult(
	xsshclient* pClient,
	xsshclientwait Wait
)
{
	xsshclientstate State = xrtSshClientState(pClient);
	xnetstream* pStream;

	if ( (Wait == XSSH_CLIENT_WAIT_READY) &&
		(State == XSSH_CLIENT_READY) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( Wait == XSSH_CLIENT_WAIT_DRAIN ) {
		pStream = xrtSshSessionStreamTcp(&pClient->Stream);
		if ( (pStream != NULL) && (xrtNetStreamPending(pStream) == 0u) ) {
			return XSSH_CLIENT_FUTURE_RESOLVE;
		}
	}
	if ( (Wait == XSSH_CLIENT_WAIT_CLOSE) &&
		(State == XSSH_CLIENT_CLOSED) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	return State >= XSSH_CLIENT_CLOSING ?
		XSSH_CLIENT_FUTURE_CLOSED : XSSH_CLIENT_FUTURE_PENDING;
}



/* 返回 channel 水平条件的当前结果。 */
static xsshclientfuturecompletion xsshClientFutureChannelResult(
	const xsshchannel* pChannel,
	xsshclientchannelwait Wait
)
{
	xsshchannelcorephase Phase = xrtSshChannelCorePhase(&pChannel->Core);

	if ( Wait == XSSH_CLIENT_CHANNEL_WAIT_OPEN ) {
		if ( Phase == XSSH_CHANNEL_CORE_OPEN ) {
			return XSSH_CLIENT_FUTURE_RESOLVE;
		}
		if ( Phase == XSSH_CHANNEL_CORE_FAILED ) {
			return XSSH_CLIENT_FUTURE_REJECT;
		}
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		xrtSshChannelCanSendData(&pChannel->Core.State) &&
		(xrtSshChannelIoWritable(&pChannel->Io) != 0u) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		(pChannel->Core.State.RemoteEof ||
		 pChannel->Core.State.RemoteClose) ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	if ( (Wait == XSSH_CLIENT_CHANNEL_WAIT_CLOSE) &&
		pChannel->Core.State.RemoteClose ) {
		return XSSH_CLIENT_FUTURE_RESOLVE;
	}
	return (Phase == XSSH_CHANNEL_CORE_FAILED) ||
		(Phase == XSSH_CHANNEL_CORE_CLOSED) ?
		XSSH_CLIENT_FUTURE_CLOSED : XSSH_CLIENT_FUTURE_PENDING;
}



/* 为远端 channel open 拒绝创建稳定结构化错误。 */
static xerror* xsshClientFutureOpenError(
	const xsshchannel* pChannel
)
{
	return xrtErrorCreate(
		XERR_IO,
		"xrt.ssh.channel.open",
		(int32)pChannel->Core.FailureReason,
		"SSH channel open was rejected"
	);
}



/* 提交一个普通客户端等待。 */
xfuture* xrtSshClientWaitAsync(
	xsshclient* pClient,
	xsshclientwait Wait
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( (Wait < XSSH_CLIENT_WAIT_READY) ||
		(Wait > XSSH_CLIENT_WAIT_CLOSE) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CLIENT;
	pWaiter->ClientWait = Wait;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	Completion = pWaiter->State->Closed ?
		(Wait == XSSH_CLIENT_WAIT_CLOSE ?
		 XSSH_CLIENT_FUTURE_RESOLVE : XSSH_CLIENT_FUTURE_CLOSED) :
		xsshClientFutureClientResult(pClient, Wait);
	if ( Completion == XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, NULL);
	}
	return pFuture;
}



/* 提交一个 channel 生命周期或写预算等待。 */
xfuture* xrtSshClientChannelWaitAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshclientchannelwait Wait
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;
	xerror* pError = NULL;

	if ( (Wait < XSSH_CLIENT_CHANNEL_WAIT_OPEN) ||
		(Wait > XSSH_CLIENT_CHANNEL_WAIT_CLOSE) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CHANNEL;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->ChannelWait = Wait;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	Completion = pWaiter->State->Closed ?
		XSSH_CLIENT_FUTURE_CLOSED :
		xsshClientFutureChannelResult(pChannel, Wait);
	if ( Completion == XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion == XSSH_CLIENT_FUTURE_REJECT ) {
		pError = xsshClientFutureOpenError(pChannel);
	}
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, pError);
	}
	xrtErrorFree(pError);
	return pFuture;
}



/* 提交一个不会消费 channel 数据的可读等待。 */
xfuture* xrtSshClientChannelReadAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
)
{
	xsshclientfuturecompletion Completion;
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( ((Stream != XSSH_CHANNEL_IO_DATA) &&
		 (Stream != XSSH_CHANNEL_IO_STDERR)) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_READ;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->Stream = Stream;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		Completion = XSSH_CLIENT_FUTURE_CLOSED;
	} else if ( xrtSshChannelIoReadable(&pChannel->Io, Stream) != 0u ) {
		Completion = XSSH_CLIENT_FUTURE_RESOLVE;
	} else if ( pChannel->Core.State.RemoteEof ||
		pChannel->Core.State.RemoteClose ) {
		Completion = XSSH_CLIENT_FUTURE_CLOSED;
	} else {
		Completion = XSSH_CLIENT_FUTURE_PENDING;
		xsshClientFutureLink(pWaiter->State, pWaiter);
	}
	(void)xrtSpinUnlock(&pWaiter->State->Lock);
	if ( Completion != XSSH_CLIENT_FUTURE_PENDING ) {
		xsshClientFutureFinish(pWaiter, Completion, NULL);
	}
	return pFuture;
}



/* 提交一个按稳定 token 精确关联的 channel request 等待。 */
xfuture* xrtSshClientChannelReplyAsync(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint64 iReplyToken
)
{
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	if ( !xrtSshClientOwnsChannel(pClient, pChannel) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY;
	pWaiter->Channel = pChannel;
	pWaiter->ChannelLocal = pChannel->Core.Local;
	pWaiter->ReplyToken = iReplyToken;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
	} else {
		xsshClientFutureLink(pWaiter->State, pWaiter);
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
	}
	return pFuture;
}



/* 提交一个按稳定 token 精确关联的 global request 等待。 */
xfuture* xrtSshClientGlobalReplyAsync(
	xsshclient* pClient,
	uint64 iReplyToken
)
{
	xsshclientfuturewaiter* pWaiter;
	xfuture* pFuture;

	pWaiter = xsshClientFutureCreate(pClient, &pFuture);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Kind = XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY;
	pWaiter->ReplyToken = iReplyToken;
	(void)xrtSpinLock(&pWaiter->State->Lock);
	if ( pWaiter->State->Closed ) {
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
		xsshClientFutureFinish(
			pWaiter,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
	} else {
		xsshClientFutureLink(pWaiter->State, pWaiter);
		(void)xrtSpinUnlock(&pWaiter->State->Lock);
	}
	return pFuture;
}



/* 判断一个等待节点是否由本次已提交信号满足。 */
static bool xsshClientFutureMatches(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	const xsshclientchannelnotice* pChannel = pNotice->ChannelNotice;
	const xsshclientglobalnotice* pGlobal = pNotice->GlobalNotice;
	bool bChannelMatches = (pWaiter->Channel == pNotice->Channel) &&
		(!pNotice->HasChannelLocal ||
		 (pWaiter->ChannelLocal == pNotice->ChannelLocal));

	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return true;
	}
	if ( pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CLIENT ) {
		return ((pWaiter->ClientWait == XSSH_CLIENT_WAIT_READY) &&
			(pNotice->Signal == XSSH_CLIENT_FUTURE_READY)) ||
			((pWaiter->ClientWait == XSSH_CLIENT_WAIT_DRAIN) &&
			 (pNotice->Signal == XSSH_CLIENT_FUTURE_DRAIN));
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_DATA) ) {
		return bChannelMatches &&
			(pWaiter->Stream == pNotice->Stream) &&
			(xrtSshChannelIoReadable(
				&pWaiter->Channel->Io,
				pWaiter->Stream
			) != 0u);
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) ) {
		return (pChannel != NULL) &&
			bChannelMatches &&
			((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
			 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED));
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_GLOBAL) ) {
		return (pGlobal != NULL) &&
			(pWaiter->ReplyToken == pGlobal->ReplyToken);
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) ) {
		return (pChannel != NULL) &&
			bChannelMatches &&
			(((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED)) ||
			 (pChannel->HasReplyToken &&
			  (pWaiter->ReplyToken == pChannel->ReplyToken) &&
			  ((pChannel->Event ==
			    XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS) ||
			   (pChannel->Event ==
			    XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE))));
	}
	if ( pWaiter->Kind != XSSH_CLIENT_FUTURE_WAIT_CHANNEL ) {
		return false;
	}
	if ( (pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		(pNotice->Signal == XSSH_CLIENT_FUTURE_WRITABLE) ) {
		return bChannelMatches;
	}
	if ( (pNotice->Signal != XSSH_CLIENT_FUTURE_CHANNEL) ||
		(pChannel == NULL) ||
		!bChannelMatches ) {
		return false;
	}
	return ((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_OPEN) &&
		((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_WRITE) &&
		((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_WRITABLE) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ||
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED) ||
		 ((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) &&
		  xrtSshChannelCanSendData(&pChannel->Channel->Core.State) &&
		  (xrtSshChannelIoWritable(&pChannel->Channel->Io) != 0u)))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		 ((pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
		  (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED))) ||
		((pWaiter->ChannelWait == XSSH_CLIENT_CHANNEL_WAIT_CLOSE) &&
		 (pChannel->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED));
}



/* 把匹配信号转换为成功、远端拒绝或连接关闭。 */
static xsshclientfuturecompletion xsshClientFutureCompletion(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CLIENT) &&
			(pWaiter->ClientWait == XSSH_CLIENT_WAIT_CLOSE) ?
			XSSH_CLIENT_FUTURE_RESOLVE :
			(pNotice->Error != NULL ?
			 XSSH_CLIENT_FUTURE_REJECT : XSSH_CLIENT_FUTURE_CLOSED);
	}
	if ( (pNotice->Signal == XSSH_CLIENT_FUTURE_CHANNEL) &&
		(pNotice->ChannelNotice != NULL) &&
		((pNotice->ChannelNotice->Event ==
		  XSSH_CLIENT_CHANNEL_EVENT_EOF) ||
		 (pNotice->ChannelNotice->Event ==
		  XSSH_CLIENT_CHANNEL_EVENT_CLOSED)) &&
		((pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_READ) ||
		 (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) ||
		 ((pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL) &&
		  (pWaiter->ChannelWait != XSSH_CLIENT_CHANNEL_WAIT_EOF) &&
		  (pWaiter->ChannelWait != XSSH_CLIENT_CHANNEL_WAIT_CLOSE))) ) {
		return XSSH_CLIENT_FUTURE_CLOSED;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL) &&
		(pNotice->ChannelNotice != NULL) &&
		(pNotice->ChannelNotice->Event ==
		 XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL_REPLY) &&
		(pNotice->ChannelNotice->Event ==
		 XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	if ( (pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY) &&
		(pNotice->GlobalNotice->Event ==
		 XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE) ) {
		return XSSH_CLIENT_FUTURE_REJECT;
	}
	return XSSH_CLIENT_FUTURE_RESOLVE;
}



/* 为远端语义拒绝创建不依赖输入 packet 的错误。 */
static xerror* xsshClientFutureNoticeError(
	const xsshclientfuturewaiter* pWaiter,
	const xsshclientfuturenotice* pNotice
)
{
	if ( pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE ) {
		return xrtErrorRef(pNotice->Error);
	}
	if ( pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_CHANNEL ) {
		return xrtErrorCreate(
			XERR_IO,
			"xrt.ssh.channel.open",
			(int32)pNotice->ChannelNotice->Reason,
			"SSH channel open was rejected"
		);
	}
	return xrtErrorCreate(
		XERR_IO,
		pWaiter->Kind == XSSH_CLIENT_FUTURE_WAIT_GLOBAL_REPLY ?
			"xrt.ssh.global.request" : "xrt.ssh.channel.request",
		(int32)XSSH_ERROR_STATE,
		"SSH request was rejected"
	);
}



/* 发布提交后信号，先从锁内摘除，再在线程安全的 Promise 路径完成。 */
void __xrtSshClientFutureNotify(
	xsshclient* pClient,
	const xsshclientfuturenotice* pNotice
)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;
	xsshclientfuturewaiter* pReadyHead = NULL;
	xsshclientfuturewaiter* pReadyTail = NULL;
	xsshclientfuturewaiter* pWaiter;
	xsshclientfuturewaiter* pNext;

	if ( pState == NULL ) {
		return;
	}
	xsshClientFutureStateRef(pState);
	(void)xrtSpinLock(&pState->Lock);
	if ( (pNotice->Signal == XSSH_CLIENT_FUTURE_CLOSE) &&
		!pState->Closed ) {
		pState->Closed = true;
	}
	pWaiter = pState->Head;
	while ( pWaiter != NULL ) {
		pNext = pWaiter->Next;
		if ( xsshClientFutureMatches(pWaiter, pNotice) ) {
			(void)xsshClientFutureRemove(pState, pWaiter);
			if ( pReadyTail != NULL ) {
				pReadyTail->Next = pWaiter;
			} else {
				pReadyHead = pWaiter;
			}
			pReadyTail = pWaiter;
		}
		pWaiter = pNext;
	}
	(void)xrtSpinUnlock(&pState->Lock);
	for ( pWaiter = pReadyHead; pWaiter != NULL; pWaiter = pNext ) {
		xsshclientfuturecompletion Completion =
			xsshClientFutureCompletion(pWaiter, pNotice);
		xerror* pError = Completion == XSSH_CLIENT_FUTURE_REJECT ?
			xsshClientFutureNoticeError(pWaiter, pNotice) : NULL;

		pNext = pWaiter->Next;
		pWaiter->Next = NULL;
		xsshClientFutureFinish(pWaiter, Completion, pError);
		xrtErrorFree(pError);
	}
	xsshClientFutureStateDestroy(pState);
}



/* Clear 先关闭全部等待，再分离客户端持有的管理器引用。 */
void __xrtSshClientFutureClear(xsshclient* pClient)
{
	xsshclientfuturestate* pState =
		(xsshclientfuturestate*)pClient->FutureState;
	xsshclientfuturewaiter* pHead;
	xsshclientfuturewaiter* pNext;

	if ( pState == NULL ) {
		return;
	}
	pClient->FutureState = NULL;
	(void)xrtSpinLock(&pState->Lock);
	pState->Closed = true;
	pHead = pState->Head;
	pState->Head = NULL;
	pState->Tail = NULL;
	for ( pNext = pHead; pNext != NULL; pNext = pNext->Next ) {
		pNext->Linked = false;
	}
	(void)xrtSpinUnlock(&pState->Lock);
	while ( pHead != NULL ) {
		pNext = pHead->Next;
		pHead->Next = NULL;
		xsshClientFutureFinish(
			pHead,
			XSSH_CLIENT_FUTURE_CLOSED,
			NULL
		);
		pHead = pNext;
	}
	xsshClientFutureStateDestroy(pState);
}

#endif
