#include <string.h>

#include <xrt/time.h>
#include <xrt/ssh_client.h>
#include <xrt/ssh_session_tcp_random.h>

#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	#include "ssh_client_future_internal.h"
#endif



#if defined(XSSH_FEATURE_CLIENT)

#define XSSH_CLIENT_GUARD UINT32_C(0x53434c54)



static void xsshClientOpen(xsshsessionstream* pStream, ptr pData);
static void xsshClientAction(
	xsshsessionstream* pStream,
	xsshsessionaction Action,
	ptr pData
);
static xsshsessionstreamdecision xsshClientIdentification(
	xsshsessionstream* pStream,
	xstrview Version,
	ptr pData
);
static xsshsessionstreamdecision xsshClientPacket(
	xsshsessionstream* pStream,
	const xsshsessiontcppacket* pPacket,
	ptr pData
);
static void xsshClientRekey(
	xsshsessionstream* pStream,
	xsshrekeydecision Decision,
	ptr pData
);
static void xsshClientError(
	xsshsessionstream* pStream,
	xsshcode Code,
	const xerror* pError,
	ptr pData
);
static void xsshClientEnd(xsshsessionstream* pStream, ptr pData);
static void xsshClientHighWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshClientLowWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
);
static void xsshClientDrain(xsshsessionstream* pStream, ptr pData);
static void xsshClientClose(
	xsshsessionstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
);
static void xsshClientReadyTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);



static const xsshsessionstreamevents xsshClientEvents = {
	xsshClientOpen,
	xsshClientAction,
	xsshClientIdentification,
	xsshClientPacket,
	xsshClientRekey,
	xsshClientError,
	xsshClientEnd,
	xsshClientHighWater,
	xsshClientLowWater,
	xsshClientDrain,
	xsshClientClose
};



/* Channel open 回调上下文只在同步构建调用期间借用。 */
typedef struct xsshclientopenbuild {
	xsshclientchannelopenproc Open;
	xsshchannel* Channel;
	ptr UserData;
} xsshclientopenbuild;



/* Channel 数据构建上下文只保存稳定 channel 和流方向。 */
typedef struct xsshclientflushbuild {
	xsshchannel* Channel;
	xsshchanneliostream Stream;
} xsshclientflushbuild;



/* Peer open 响应只在读提交后的同步构建期间存在。 */
typedef struct xsshclientopenresponse {
	xsshchannel* Channel;
	xsshclientchanneldecision Decision;
	uint32 Reason;
} xsshclientopenresponse;



/* 验证客户端哨兵和资源生命周期，不触碰尚未初始化的动态对象。 */
static bool xsshClientValid(const xsshclient* pClient)
{
	return xrtMemRangeValid(pClient, sizeof(*pClient)) &&
		(pClient->Guard == XSSH_CLIENT_GUARD) &&
		(pClient->State >= XSSH_CLIENT_CREATED) &&
		(pClient->State <= XSSH_CLIENT_CLOSED);
}



/* 验证调用发生在已附着 Stream 的所属 Worker。 */
static bool xsshClientCurrent(const xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;

	if ( !xsshClientValid(pClient) ) {
		return false;
	}
	pStream = pClient->Stream.Stream;
	if ( pStream == NULL ) {
		return false;
	}
	pWorker = xrtNetStreamWorker(pStream);
	return (pWorker != NULL) && xrtNetWorkerIsCurrent(pWorker);
}



/* 验证 channel 来自当前客户端的动态集合。 */
static bool xsshClientChannelOwned(
	const xsshclient* pClient,
	const xsshchannel* pChannel
)
{
	if ( !xsshClientValid(pClient) || !pClient->ResourcesReady ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!pChannel->Initialized ) {
		return false;
	}
	return xrtSshChannelsConstGet(
		&pClient->Channels,
		pChannel->Core.Local
	) == pChannel;
}



/* 返回当前未提交 peer CHANNEL_OPEN，其他 packet 不可伪造决定。 */
static const xsshchannelopen* xsshClientCurrentOpen(
	xsshclient* pClient
)
{
	const xsshsessiontcppacket* pPacket;

	if ( !xsshClientCurrent(pClient) ) {
		return NULL;
	}
	if ( pClient->OpenCurrent != NULL ) {
		return pClient->OpenCurrent;
	}
	pPacket = xrtSshSessionStreamPacket(&pClient->Stream);
	return (pPacket != NULL) &&
		(pPacket->Session.Kind == XSSH_SESSION_PACKET_CONNECTION) &&
		(pPacket->Session.Message.Connection.Kind ==
		 XSSH_CONNECTION_PACKET_CHANNEL_OPEN) ?
		&pPacket->Session.Message.Connection.Message.ChannelOpen : NULL;
}



/* 丢弃尚未进入写事务的 peer open 决定。 */
static void xsshClientOpenPendingDiscard(xsshclient* pClient)
{
	xsshchannel* pChannel = pClient->OpenPendingChannel;

	pClient->OpenPendingChannel = NULL;
	pClient->OpenDecision = XSSH_CLIENT_CHANNEL_NONE;
	pClient->OpenReason = 0u;
	if ( pChannel != NULL ) {
		(void)xrtSshChannelsDiscard(
			&pClient->Channels,
			pChannel->Core.Local
		);
	}
}



/* 暂存 peer open 决定，并为 confirmation/failure 保留稳定 channel。 */
static xsshcode xsshClientOpenStage(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshclientchanneldecision Decision,
	uint32 iReason,
	xsshchannel** ppChannel
)
{
	const xsshchannelopen* pCurrent = xsshClientCurrentOpen(pClient);
	xsshchannel* pChannel;
	xsshcode Code;

	if ( (pCurrent == NULL) || (pOpen != pCurrent) ||
		(pClient->OpenDecision != XSSH_CLIENT_CHANNEL_NONE) ||
		(pClient->OpenPendingChannel != NULL) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelsAccept(
		&pClient->Channels,
		pOpen,
		&pChannel
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pClient->OpenPendingChannel = pChannel;
	pClient->OpenDecision = Decision;
	pClient->OpenReason = iReason;
	if ( ppChannel != NULL ) {
		*ppChannel = pChannel;
	}
	return XSSH_OK;
}



/* 把公开类型化 open 回调适配为通用控制报文构建器。 */
static xsshcode xsshClientChannelOpenBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientopenbuild* pBuild = (xsshclientopenbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		(pBuild->Open == NULL) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return pBuild->Open(
		pWriter,
		&pBuild->Channel->Core,
		pBuild->UserData
	);
}



/* 从 channel I/O 队首构建一条暂存发送事务。 */
static xsshcode xsshClientChannelFlushBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientflushbuild* pBuild = (xsshclientflushbuild*)pData;
	xbytesview Payload;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshChannelIoSendPrepare(
		&pBuild->Channel->Io,
		pBuild->Stream,
		pWriter,
		&Payload
	);
}



/* 从 channel 当前远端编号构建窗口返还。 */
static xsshcode xsshClientChannelAdjustBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;
	uint32 iBytes;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	iBytes = xrtSshChannelCoreAdjustLimit(&pChannel->Core);
	return iBytes != 0u ? xrtSshChannelWindowAdjustWrite(
		pWriter,
		iRemote,
		iBytes
	) : XSSH_NEED_MORE;
}



/* 从 channel 当前远端编号构建 EOF。 */
static xsshcode xsshClientChannelEofBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	return xrtSshChannelEofWrite(pWriter, iRemote);
}



/* 从 channel 当前远端编号构建 CLOSE。 */
static xsshcode xsshClientChannelCloseBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshchannel* pChannel = (xsshchannel*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtSshChannelCoreIds(&pChannel->Core, &iLocal, &iRemote) ) {
		return XSSH_ERROR_STATE;
	}
	(void)iLocal;
	return xrtSshChannelCloseWrite(pWriter, iRemote);
}



/* 在 peer open 读事务完成后构建 confirmation 或 failure。 */
static xsshcode xsshClientOpenResponseBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientopenresponse* pBuild = (xsshclientopenresponse*)pData;
	uint32 iLocal;
	uint32 iRemote;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pBuild->Channel, sizeof(*pBuild->Channel)) ||
		!xrtSshChannelCoreIds(
			&pBuild->Channel->Core,
			&iLocal,
			&iRemote
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pBuild->Decision == XSSH_CLIENT_CHANNEL_ACCEPT ) {
		return xrtSshChannelOpenConfirmationWrite(
			pWriter,
			iRemote,
			iLocal,
			pBuild->Channel->Core.Window.ReceiveWindow,
			pBuild->Channel->Core.Window.ReceiveMaxPacket,
			(xbytesview){ NULL, 0u }
		);
	}
	if ( pBuild->Decision == XSSH_CLIENT_CHANNEL_REJECT ) {
		return xrtSshChannelOpenFailureWrite(
			pWriter,
			iRemote,
			pBuild->Reason,
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL("")
		);
	}
	return XSSH_ERROR_STATE;
}



/* 写事务提交后开放被接受 channel，或回收已拒绝 channel。 */
static xsshcode xsshClientOpenSendCommit(xsshclient* pClient)
{
	xsshchannel* pChannel = pClient->OpenSendChannel;
	xsshclientchannelnotice Notice;
	xsshchannelcorephase Phase;
	uint32 iLocal;

	if ( pChannel == NULL ) {
		return XSSH_OK;
	}
	iLocal = pChannel->Core.Local;
	Phase = xrtSshChannelCorePhase(&pChannel->Core);
	if ( Phase == XSSH_CHANNEL_CORE_ACCEPTING ) {
		return XSSH_OK;
	}
	pClient->OpenSendChannel = NULL;
	if ( Phase == XSSH_CHANNEL_CORE_OPEN ) {
		memset(&Notice, 0, sizeof(Notice));
		Notice.Channel = pChannel;
		Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPENED;
		Notice.Incoming = true;
		if ( pClient->Events.Channel != NULL ) {
			pClient->Events.Channel(
				pClient,
				&Notice,
				pClient->UserData
			);
		}
		return XSSH_OK;
	}
	if ( Phase == XSSH_CHANNEL_CORE_FAILED ) {
		return xrtSshChannelsDiscard(
			&pClient->Channels,
			iLocal
		) ? XSSH_OK : XSSH_ERROR_STATE;
	}
	return XSSH_ERROR_STATE;
}



/* 把已提交 peer open 决定转为唯一线路写事务。 */
static xsshcode xsshClientOpenResponse(xsshclient* pClient)
{
	xsshclientopenresponse Build;
	xsshchannel* pChannel;
	xsshcode Code;

	if ( pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE ) {
		return XSSH_OK;
	}
	pChannel = pClient->OpenPendingChannel;
	if ( pChannel == NULL ) {
		return XSSH_ERROR_STATE;
	}
	Build.Channel = pChannel;
	Build.Decision = pClient->OpenDecision;
	Build.Reason = pClient->OpenReason;
	pClient->OpenPendingChannel = NULL;
	pClient->OpenDecision = XSSH_CLIENT_CHANNEL_NONE;
	pClient->OpenReason = 0u;
	pClient->OpenSendChannel = pChannel;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientOpenResponseBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( Code != XSSH_OK ) {
		pClient->OpenSendChannel = NULL;
		(void)xrtSshChannelsDiscard(
			&pClient->Channels,
			pChannel->Core.Local
		);
	}
	return Code;
}



/* 把客户端层错误发布给用户，但不覆盖底层已经存在的结构化错误。 */
static void xsshClientErrorReport(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage,
	bool bTerminal
)
{
	const xerror* pError = xrtGetError();
	bool bRelevant = (pError != NULL) && (
		(xrtErrorFind(pError, "xrt.ssh", (int32)Code) != NULL) ||
		(((Kind == XERR_IO) || (Kind == XERR_MEMORY)) &&
		 (xrtErrorIs(pError, Kind) != NULL))
	);

	if ( !bRelevant ) {
		xrtSetErrorInfo(Kind, "xrt.ssh.client", (int32)Code, sMessage);
		pError = xrtGetError();
	}
	if ( bTerminal && xsshClientValid(pClient) &&
		(pClient->TerminalError == NULL) && (pError != NULL) ) {
		pClient->TerminalError = xrtErrorRef(pError);
	}
	if ( xsshClientValid(pClient) && (pClient->Events.Error != NULL) ) {
		pClient->Events.Error(
			pClient,
			Code,
			pError,
			pClient->UserData
		);
	}
}



/* 发布会终结连接或其全部未决操作的错误。 */
static void xsshClientErrorNotify(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xsshClientErrorReport(pClient, Code, Kind, sMessage, true);
}



/* 发布可在同一输入事务上显式重试的错误，不污染未来关闭终态。 */
static void xsshClientRetryNotify(
	xsshclient* pClient,
	xsshcode Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xsshClientErrorReport(pClient, Code, Kind, sMessage, false);
}



/* 取消就绪截止时间；Timer 的取消回调会因 ID 已清零而成为空操作。 */
static void xsshClientReadyTimerCancel(xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	xnetengine* pEngine;
	uint64 Id;

	if ( pClient->ReadyTimer == 0u ) {
		return;
	}
	pStream = xrtSshSessionStreamTcp(&pClient->Stream);
	pWorker = pStream != NULL ? xrtNetStreamWorker(pStream) : NULL;
	pEngine = pWorker != NULL ? xrtNetWorkerEngine(pWorker) : NULL;
	Id = pClient->ReadyTimer;
	pClient->ReadyTimer = 0u;
	if ( pEngine != NULL ) {
		(void)xrtNetEngineTimerCancelCurrent(pEngine, Id);
	}
}



/* 为 TCP 建连后的 SSH identification、KEX 和认证建立统一截止时间。 */
static bool xsshClientReadyTimerStart(xsshclient* pClient)
{
	xnetstream* pStream;
	xnetworker* pWorker;
	xnetengine* pEngine;
	uint64 Id;

	if ( pClient->Config.ReadyTimeout == 0u ) {
		return true;
	}
	pStream = xrtSshSessionStreamTcp(&pClient->Stream);
	pWorker = pStream != NULL ? xrtNetStreamWorker(pStream) : NULL;
	pEngine = pWorker != NULL ? xrtNetWorkerEngine(pWorker) : NULL;
	if ( pEngine == NULL ) {
		return false;
	}
	Id = xrtNetEngineAfter(
		pEngine,
		xrtNetWorkerIndex(pWorker),
		pClient->Config.ReadyTimeout,
		xsshClientReadyTimer,
		pClient
	);
	if ( Id == 0u ) {
		return false;
	}
	pClient->ReadyTimer = Id;
	return true;
}



/* 就绪截止时间只终结仍处于握手阶段的同一客户端生命周期。 */
static void xsshClientReadyTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pWorker;
	if ( !xsshClientValid(pClient) ||
		(pClient->ReadyTimer != Id) ) {
		return;
	}
	pClient->ReadyTimer = 0u;
	if ( (Result == XNET_RESULT_CANCELLED) ||
		(Result == XNET_RESULT_CLOSED) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return;
	}
	if ( Result == XNET_RESULT_OK ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_TIMEOUT,
			XERR_TIMEOUT,
			"SSH client did not become ready before its deadline"
		);
	} else {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_STATE,
			XERR_STATE,
			"SSH client ready timer terminated unexpectedly"
		);
	}
	pClient->State = XSSH_CLIENT_CLOSING;
	(void)xrtSshSessionStreamAbort(&pClient->Stream);
}



/* 把正常等待输入的 Drive 返回值收敛为调用成功。 */
static xsshcode xsshClientDrive(xsshclient* pClient)
{
	xsshcode Code = xrtSshSessionStreamDrive(&pClient->Stream);

	return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
}



/* 在 packet 提交后原子发布 channel I/O，再通知应用读取。 */
static xsshcode xsshClientReceiveCommit(xsshclient* pClient)
{
	xsshchannel* pChannel;
	xsshchanneliostream Stream;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		uint32 iLocal;
	#endif

	if ( !pClient->ReceivePending ) {
		return XSSH_OK;
	}
	pChannel = pClient->ReceiveChannel;
	Stream = pClient->ReceiveStream;
	pClient->ReceiveChannel = NULL;
	pClient->ReceivePending = false;
	if ( (pChannel == NULL) ||
		(xrtSshChannelIoReceiveCommit(&pChannel->Io) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		iLocal = pChannel->Core.Local;
	#endif
	if ( pClient->Events.Data != NULL ) {
		pClient->Events.Data(
			pClient,
			pChannel,
			Stream,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_DATA;
		FutureNotice.Channel = pChannel;
		FutureNotice.Stream = Stream;
		FutureNotice.ChannelLocal = iLocal;
		FutureNotice.HasChannelLocal = true;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 把已提交 connection 状态转换为不借用 packet 的稳定通知。 */
static xsshcode xsshClientChannelNoticeCommit(xsshclient* pClient)
{
	xsshclientchannelnotice Notice;
	xsshchannelcorephase Phase;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		uint32 iLocal;
	#endif

	if ( !pClient->ChannelNoticePending ) {
		return XSSH_OK;
	}
	Notice = pClient->ChannelNotice;
	memset(&pClient->ChannelNotice, 0, sizeof(pClient->ChannelNotice));
	pClient->ChannelNoticePending = false;
	if ( !xsshClientChannelOwned(pClient, Notice.Channel) ) {
		return XSSH_ERROR_STATE;
	}
	Phase = xrtSshChannelCorePhase(&Notice.Channel->Core);
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		iLocal = Notice.Channel->Core.Local;
	#endif
	if ( ((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED) &&
		 (Phase != XSSH_CHANNEL_CORE_OPEN)) ||
		((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED) &&
		 (Phase != XSSH_CHANNEL_CORE_FAILED)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pClient->Events.Channel != NULL ) {
		pClient->Events.Channel(
			pClient,
			&Notice,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_CHANNEL;
		FutureNotice.Channel = Notice.Channel;
		FutureNotice.ChannelNotice = &Notice;
		FutureNotice.ChannelLocal = iLocal;
		FutureNotice.HasChannelLocal = true;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 在全局回复和对应 FIFO 出队提交后发布稳定 token。 */
static xsshcode xsshClientGlobalNoticeCommit(xsshclient* pClient)
{
	xsshclientglobalnotice Notice;

	if ( !pClient->GlobalNoticePending ) {
		return XSSH_OK;
	}
	Notice = pClient->GlobalNotice;
	memset(&pClient->GlobalNotice, 0, sizeof(pClient->GlobalNotice));
	pClient->GlobalNoticePending = false;
	if ( pClient->Events.Global != NULL ) {
		pClient->Events.Global(
			pClient,
			&Notice,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_GLOBAL;
		FutureNotice.GlobalNotice = &Notice;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
	return XSSH_OK;
}



/* 在 SSH 与 transport 写事务提交后消费 channel I/O 队首。 */
static xsshcode xsshClientSendCommit(xsshclient* pClient)
{
	xsshchannel* pChannel;
	xsshcode Code;

	if ( !pClient->SendPending ) {
		return XSSH_OK;
	}
	pChannel = pClient->SendChannel;
	pClient->SendChannel = NULL;
	pClient->SendPending = false;
	Code = pChannel != NULL ?
		xrtSshChannelIoSendCommit(&pChannel->Io) : XSSH_ERROR_STATE;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	if ( Code == XSSH_OK ) {
		pClient->FutureWritableChannel = pChannel;
	}
	#endif
	return Code;
}



/* 为需要提交后发布的 channel 状态保存稳定对象和标量。 */
static xsshcode xsshClientChannelNoticePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection;
	xsshclientchannelnotice Notice;
	uint32 iRecipient;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	pConnection = &pPacket->Session.Message.Connection;
	memset(&Notice, 0, sizeof(Notice));
	switch ( pConnection->Kind ) {
		case XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION:
			iRecipient = pConnection->Message.ChannelConfirmation.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPENED;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE:
			iRecipient = pConnection->Message.ChannelOpenFailure.Recipient;
			Notice.Reason = pConnection->Message.ChannelOpenFailure.Reason;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_ADJUST:
			iRecipient = pConnection->Message.ChannelAdjust.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_WRITABLE;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS;
			Notice.ReplyToken = pConnection->ReplyToken;
			Notice.HasReplyToken = pConnection->HasReplyToken;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_FAILURE:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE;
			Notice.ReplyToken = pConnection->ReplyToken;
			Notice.HasReplyToken = pConnection->HasReplyToken;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_EOF:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_EOF;
			break;
		case XSSH_CONNECTION_PACKET_CHANNEL_CLOSE:
			iRecipient = pConnection->Message.Recipient;
			Notice.Event = XSSH_CLIENT_CHANNEL_EVENT_CLOSED;
			break;
		default:
			return XSSH_OK;
	}
	if ( pClient->ChannelNoticePending ) {
		return XSSH_ERROR_STATE;
	}
	Notice.Channel = xrtSshChannelsGet(&pClient->Channels, iRecipient);
	if ( (Notice.Channel == NULL) ||
		(((Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS) ||
		  (Notice.Event == XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE)) &&
		 !Notice.HasReplyToken) ) {
		return XSSH_ERROR_STATE;
	}
	pClient->ChannelNotice = Notice;
	pClient->ChannelNoticePending = true;
	return XSSH_OK;
}



/* 为全局 success/failure 保存已关联但尚未提交的回复 token。 */
static xsshcode xsshClientGlobalNoticePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection;
	xsshclientglobalnotice Notice;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	pConnection = &pPacket->Session.Message.Connection;
	if ( pConnection->Kind == XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS ) {
		Notice.Event = XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS;
	} else if ( pConnection->Kind ==
		XSSH_CONNECTION_PACKET_GLOBAL_FAILURE ) {
		Notice.Event = XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE;
	} else {
		return XSSH_OK;
	}
	if ( pClient->GlobalNoticePending || !pConnection->HasReplyToken ) {
		return XSSH_ERROR_STATE;
	}
	Notice.ReplyToken = pConnection->ReplyToken;
	pClient->GlobalNotice = Notice;
	pClient->GlobalNoticePending = true;
	return XSSH_OK;
}



/* 为 DATA 或标准 stderr 准备动态接收，未知扩展仍保留给 Packet 回调。 */
static xsshcode xsshClientReceivePrepare(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	const xsshconnectionpacket* pConnection =
		&pPacket->Session.Message.Connection;
	xsshchanneliostream Stream;
	xbytesview Data;
	uint32 iRecipient;
	xsshchannel* pChannel;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_OK;
	}
	if ( pConnection->Kind == XSSH_CONNECTION_PACKET_CHANNEL_DATA ) {
		iRecipient = pConnection->Message.ChannelData.Recipient;
		Data = pConnection->Message.ChannelData.Data;
		Stream = XSSH_CHANNEL_IO_DATA;
	} else if ( (pConnection->Kind ==
		XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA) &&
		(pConnection->Message.ChannelExtendedData.Type ==
		 XSSH_CHANNEL_EXTENDED_DATA_STDERR) ) {
		iRecipient = pConnection->Message.ChannelExtendedData.Recipient;
		Data = pConnection->Message.ChannelExtendedData.Data;
		Stream = XSSH_CHANNEL_IO_STDERR;
	} else {
		return XSSH_OK;
	}
	pChannel = xrtSshChannelsGet(&pClient->Channels, iRecipient);
	if ( pChannel == NULL ) {
		return XSSH_ERROR_STATE;
	}
	pClient->ReceiveChannel = pChannel;
	pClient->ReceiveStream = Stream;
	return xrtSshChannelIoReceivePrepare(
		&pChannel->Io,
		Stream,
		iRecipient,
		Data
	);
}



/* 回滚当前 packet 建立的全部内部暂存，保持拒绝和异常路径一致。 */
static void xsshClientPacketStageAbort(xsshclient* pClient)
{
	if ( (pClient->ReceiveChannel != NULL) &&
		(pClient->ReceiveChannel->Io.Pending ==
		 XSSH_CHANNEL_IO_PENDING_RECEIVE) ) {
		(void)xrtSshChannelIoReceiveAbort(&pClient->ReceiveChannel->Io);
	}
	pClient->ReceiveChannel = NULL;
	pClient->ReceivePending = false;
	memset(&pClient->ChannelNotice, 0, sizeof(pClient->ChannelNotice));
	pClient->ChannelNoticePending = false;
	memset(&pClient->GlobalNotice, 0, sizeof(pClient->GlobalNotice));
	pClient->GlobalNoticePending = false;
	pClient->OpenCurrent = NULL;
	xsshClientOpenPendingDiscard(pClient);
}



/* 处理一个已认证 packet 的内部准备和用户提交决定。 */
static xsshsessionstreamdecision xsshClientPacketDecide(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket
)
{
	xsshsessionstreamdecision Decision = XSSH_SESSION_STREAM_ACCEPT;
	bool bChannelOpen;
	xsshcode Code;

	bChannelOpen = (pPacket->Session.Kind ==
		XSSH_SESSION_PACKET_CONNECTION) &&
		(pPacket->Session.Message.Connection.Kind ==
		 XSSH_CONNECTION_PACKET_CHANNEL_OPEN);
	pClient->OpenCurrent = bChannelOpen ?
		&pPacket->Session.Message.Connection.Message.ChannelOpen : NULL;
	Code = xrtSshClientCoreObserve(&pClient->Core, &pClient->Stream.Session,
		pPacket);
	if ( Code == XSSH_OK ) {
		Code = xsshClientReceivePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshClientChannelNoticePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshClientGlobalNoticePrepare(pClient, pPacket);
	}
	if ( Code == XSSH_ERROR_SPACE ) {
		pClient->OpenCurrent = NULL;
		pClient->ReceiveRetry = true;
		xsshClientRetryNotify(
			pClient,
			Code,
			XERR_MEMORY,
			"SSH client needs memory before retrying channel input"
		);
		return XSSH_SESSION_STREAM_HOLD;
	}
	if ( Code != XSSH_OK ) {
		xsshClientPacketStageAbort(pClient);
		xsshClientErrorNotify(
			pClient,
			Code,
			XERR_PROTOCOL,
			"SSH client rejected peer packet"
		);
		return XSSH_SESSION_STREAM_ABORT;
	}
	pClient->ReceivePending = pClient->ReceiveChannel != NULL;
	if ( pClient->Events.Packet != NULL ) {
		Decision = pClient->Events.Packet(
			pClient,
			pPacket,
			pClient->UserData
		);
	}
	if ( (Decision == XSSH_SESSION_STREAM_ACCEPT) && bChannelOpen &&
		(pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE) ) {
		Code = xsshClientOpenStage(
			pClient,
			&pPacket->Session.Message.Connection.Message.ChannelOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);
		if ( Code == XSSH_ERROR_SPACE ) {
			pClient->OpenCurrent = NULL;
			pClient->ReceiveRetry = true;
			pClient->ReceiveRetryOpen = true;
			xsshClientRetryNotify(
				pClient,
				Code,
				XERR_MEMORY,
				"SSH client needs memory before rejecting peer channel"
			);
			return XSSH_SESSION_STREAM_HOLD;
		}
		if ( Code != XSSH_OK ) {
			pClient->OpenCurrent = NULL;
			xsshClientErrorNotify(
				pClient,
				Code,
				XERR_STATE,
				"SSH client could not stage peer channel decision"
			);
			return XSSH_SESSION_STREAM_ABORT;
		}
	}
	pClient->OpenCurrent = NULL;
	if ( (Decision != XSSH_SESSION_STREAM_ACCEPT) &&
		(Decision != XSSH_SESSION_STREAM_HOLD) ) {
		xsshClientPacketStageAbort(pClient);
		return XSSH_SESSION_STREAM_ABORT;
	}
	return Decision;
}



/* 推进握手核心并把唯一输出事务交给 SessionStream。 */
static xsshcode xsshClientAdvance(xsshclient* pClient)
{
	xsshsessiontcp* pSession = xrtSshSessionStreamSession(&pClient->Stream);
	xsshsessionreader* pReader = xrtSshSessionStreamReader(&pClient->Stream);
	xsshsessionpacketkind Kind;
	xsshclientnext Next;
	xsshcode Code;

	if ( (pSession == NULL) || (pReader == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreNext(
		&pClient->Core,
		pSession,
		pReader,
		xrtClock() / 1000u,
		&Next
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_IDENTIFICATION ) {
		return xrtSshSessionTcpIdentificationWritePrepare(
			pSession,
			Next.Text
		);
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD ) {
		return xrtSshSessionTcpWritePrepare(
			pSession,
			Next.Data,
			NULL,
			NULL,
			0u,
			xrtClock() / 1000u,
			&Kind
		);
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_HOST_KEY ) {
		if ( !pClient->HostNotified &&
			(pClient->Events.HostKey != NULL) ) {
			pClient->HostNotified = true;
			pClient->Events.HostKey(pClient, pClient->UserData);
		}
		return XSSH_OK;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_AUTH ) {
		if ( !pClient->AuthNotified &&
			(pClient->Events.Authenticate != NULL) ) {
			pClient->AuthNotified = true;
			pClient->Events.Authenticate(pClient, pClient->UserData);
		}
		return XSSH_OK;
	}
	if ( Next.Kind == XSSH_CLIENT_NEXT_READY ) {
		xsshClientReadyTimerCancel(pClient);
		pClient->State = XSSH_CLIENT_READY;
		if ( !pClient->ReadyNotified ) {
			pClient->ReadyNotified = true;
			if ( pClient->Events.Ready != NULL ) {
				pClient->Events.Ready(pClient, pClient->UserData);
			}
			#if defined(XSSH_FEATURE_CLIENT_FUTURE)
			{
				xsshclientfuturenotice FutureNotice;

				memset(&FutureNotice, 0, sizeof(FutureNotice));
				FutureNotice.Signal = XSSH_CLIENT_FUTURE_READY;
				__xrtSshClientFutureNotify(pClient, &FutureNotice);
			}
			#endif
		}
	} else if ( Next.Kind == XSSH_CLIENT_NEXT_CLOSING ) {
		xsshClientReadyTimerCancel(pClient);
		pClient->State = XSSH_CLIENT_CLOSING;
	}
	return XSSH_OK;
}



/* 初始化 Worker 关联资源后发布连接打开。 */
static void xsshClientOpen(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;
	xnetstream* pTcp;
	xnetworker* pWorker;
	xnetbufpool* pPool;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	pTcp = xrtSshSessionStreamTcp(pStream);
	pWorker = pTcp != NULL ? xrtNetStreamWorker(pTcp) : NULL;
	pPool = pWorker != NULL ? xrtNetWorkerBufPool(pWorker) : NULL;

	if ( pPool == NULL ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client Worker resources could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	if ( !xrtSshChannelsInit(
		&pClient->Channels,
		pPool,
		&pClient->Config.Channels
	) ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client channels could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	if ( !xrtNetBufInit(&pClient->Control, pPool) ) {
		xrtSshChannelsClear(&pClient->Channels);
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client control buffer could not initialize"
		);
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	pClient->ResourcesReady = true;
	pClient->State = XSSH_CLIENT_HANDSHAKE;
	if ( !xsshClientReadyTimerStart(pClient) ) {
		xsshClientErrorNotify(
			pClient,
			XSSH_ERROR_SPACE,
			XERR_MEMORY,
			"SSH client ready timer could not initialize"
		);
		pClient->State = XSSH_CLIENT_CLOSING;
		(void)xrtSshSessionStreamAbort(pStream);
		return;
	}
	if ( pClient->Events.Open != NULL ) {
		pClient->Events.Open(pClient, pClient->UserData);
	}
}



/* 在 Action 边界提交 staged DATA，再推进客户端握手动作。 */
static void xsshClientAction(
	xsshsessionstream* pStream,
	xsshsessionaction Action,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;
	xsshcode Code;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return;
	}
	if ( Action == XSSH_SESSION_ACTION_CONNECTION ) {
		Code = xsshClientSendCommit(pClient);
		if ( Code == XSSH_OK ) {
			Code = xsshClientOpenSendCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientOpenResponse(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientReceiveCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientChannelNoticeCommit(pClient);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshClientGlobalNoticeCommit(pClient);
		}
		#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		if ( (Code == XSSH_OK) &&
			(pClient->FutureWritableChannel != NULL) ) {
			xsshclientfuturenotice FutureNotice;

			memset(&FutureNotice, 0, sizeof(FutureNotice));
			FutureNotice.Signal = XSSH_CLIENT_FUTURE_WRITABLE;
			FutureNotice.Channel = pClient->FutureWritableChannel;
			FutureNotice.ChannelLocal =
				pClient->FutureWritableChannel->Core.Local;
			FutureNotice.HasChannelLocal = true;
			pClient->FutureWritableChannel = NULL;
			__xrtSshClientFutureNotify(pClient, &FutureNotice);
		}
		#endif
		if ( Code != XSSH_OK ) {
			#if defined(XSSH_FEATURE_CLIENT_FUTURE)
				pClient->FutureWritableChannel = NULL;
			#endif
			xsshClientErrorNotify(
				pClient,
				Code,
				XERR_STATE,
				"SSH client channel transaction diverged"
			);
			(void)xrtSshSessionStreamAbort(pStream);
			return;
		}
	}
	Code = xsshClientAdvance(pClient);
	if ( Code != XSSH_OK ) {
		xsshClientErrorNotify(
			pClient,
			Code,
			Code == XSSH_ERROR_SPACE ? XERR_MEMORY : XERR_PROTOCOL,
			"SSH client action failed"
		);
		(void)xrtSshSessionStreamAbort(pStream);
	}
}



/* 客户端接受所有已经通过底层格式检查的 SSH-2.0 identification。 */
static xsshsessionstreamdecision xsshClientIdentification(
	xsshsessionstream* pStream,
	xstrview Version,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)Version;
	return xsshClientValid(pClient) && (pStream == &pClient->Stream) ?
		XSSH_SESSION_STREAM_ACCEPT : XSSH_SESSION_STREAM_ABORT;
}



/* 在底层提交前完成认证观察、channel DATA 预留和用户决策。 */
static xsshsessionstreamdecision xsshClientPacket(
	xsshsessionstream* pStream,
	const xsshsessiontcppacket* pPacket,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return XSSH_SESSION_STREAM_ABORT;
	}
	return xsshClientPacketDecide(pClient, pPacket);
}



/* 转发非空 rekey 建议。 */
static void xsshClientRekey(
	xsshsessionstream* pStream,
	xsshrekeydecision Decision,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Rekey != NULL) ) {
		pClient->Events.Rekey(
			pClient,
			Decision,
			pClient->UserData
		);
	}
}



/* 转发底层 SessionStream 的结构化错误。 */
static void xsshClientError(
	xsshsessionstream* pStream,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Error != NULL) ) {
		pClient->Events.Error(
			pClient,
			Code,
			pError,
			pClient->UserData
		);
	}
}



/* 转发 peer TCP EOF，channel EOF 仍由协议 packet 表达。 */
static void xsshClientEnd(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.End != NULL) ) {
		pClient->Events.End(pClient, pClient->UserData);
	}
}



/* 转发 TCP 高水位通知。 */
static void xsshClientHighWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) &&
		(pClient->Events.HighWater != NULL) ) {
		pClient->Events.HighWater(
			pClient,
			iQueued,
			pClient->UserData
		);
	}
}



/* 转发 TCP 低水位通知。 */
static void xsshClientLowWater(
	xsshsessionstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) &&
		(pClient->Events.LowWater != NULL) ) {
		pClient->Events.LowWater(
			pClient,
			iQueued,
			pClient->UserData
		);
	}
}



/* 转发 TCP 发送队列排空通知。 */
static void xsshClientDrain(xsshsessionstream* pStream, ptr pData)
{
	xsshclient* pClient = (xsshclient*)pData;

	(void)pStream;
	if ( xsshClientValid(pClient) && (pClient->Events.Drain != NULL) ) {
		pClient->Events.Drain(pClient, pClient->UserData);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	if ( xsshClientValid(pClient) ) {
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_DRAIN;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
}



/* 发布唯一关闭终态，动态资源保留到显式 Clear。 */
static void xsshClientClose(
	xsshsessionstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xsshclient* pClient = (xsshclient*)pData;
	const xerror* pTerminal;

	if ( !xsshClientValid(pClient) || (pStream != &pClient->Stream) ) {
		return;
	}
	xsshClientReadyTimerCancel(pClient);
	pTerminal = pError != NULL ? pError : pClient->TerminalError;
	pClient->State = XSSH_CLIENT_CLOSED;
	if ( pClient->Events.Close != NULL ) {
		pClient->Events.Close(
			pClient,
			Result,
			pTerminal,
			pClient->UserData
		);
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
	{
		xsshclientfuturenotice FutureNotice;

		memset(&FutureNotice, 0, sizeof(FutureNotice));
		FutureNotice.Signal = XSSH_CLIENT_FUTURE_CLOSE;
		FutureNotice.Error = pTerminal;
		__xrtSshClientFutureNotify(pClient, &FutureNotice);
	}
	#endif
}



/* 写入安全、无隐藏运行时的客户端默认配置。 */
bool xrtSshClientConfigInit(xsshclientconfig* pConfig)
{
	xsshclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(&Config, 0, sizeof(Config));
	if ( !xrtSshClientCoreConfigInit(&Config.Core) ) {
		return false;
	}
	xrtSshChannelsConfigInit(&Config.Channels);
	Config.ReadyTimeout = XSSH_CLIENT_READY_TIMEOUT_DEFAULT;
	Config.ControlInitial = XSSH_CLIENT_CONTROL_INITIAL_DEFAULT;
	Config.ControlLimit = XSSH_CLIENT_CONTROL_LIMIT_DEFAULT;
	Config.GlobalReplyLimit = XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT;
	*pConfig = Config;
	return true;
}



/* 初始化客户端组合对象，不创建网络或动态 channel 块。 */
bool xrtSshClientInit(
	xsshclient* pClient,
	const xsshclientconfig* pConfig,
	const xsshclientevents* pEvents,
	ptr pData
)
{
	xsshsessiontcpconfig SessionConfig;
	xsshclientevents Events;

	if ( !xrtMemRangeValid(pClient, sizeof(*pClient)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			pConfig,
			sizeof(*pConfig)
		) || (pConfig->Core.Kex.Role != XSSH_ROLE_CLIENT) ||
		(pConfig->ControlInitial == 0u) ||
		(pConfig->ControlInitial > pConfig->ControlLimit) ||
		(pConfig->GlobalReplyLimit == 0u) ) {
		return false;
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !xrtMemRangeValid(pEvents, sizeof(*pEvents)) ||
			xrtMemRangesOverlap(
				pClient,
				sizeof(*pClient),
				pEvents,
				sizeof(*pEvents)
			) ) {
			return false;
		}
		Events = *pEvents;
	}
	memset(pClient, 0, sizeof(*pClient));
	pClient->Config = *pConfig;
	pClient->Events = Events;
	pClient->UserData = pData;
	pClient->ControlTarget = pConfig->ControlInitial;
	pClient->State = XSSH_CLIENT_CREATED;
	pClient->Guard = XSSH_CLIENT_GUARD;
	if ( !xrtSshReplyQueueInit(&pClient->GlobalReplies, NULL, 0u) ||
		!xrtSshClientCoreInit(&pClient->Core, &pConfig->Core) ||
		!xrtSshSessionTcpConfigInit(
			&SessionConfig,
			XSSH_ROLE_CLIENT
		) ) {
		xrtSshClientCoreClear(&pClient->Core);
		memset(pClient, 0, sizeof(*pClient));
		return false;
	}
	SessionConfig.ChannelResolve = xrtSshChannelsResolve;
	SessionConfig.ChannelUserData = &pClient->Channels;
	SessionConfig.GlobalReplies = &pClient->GlobalReplies;
	if ( !xrtSshSessionStreamInit(
		&pClient->Stream,
		&SessionConfig,
		&xsshClientEvents,
		pClient
	) ) {
		xrtSshClientCoreClear(&pClient->Core);
		memset(pClient, 0, sizeof(*pClient));
		return false;
	}
	return true;
}



/* 清理关闭客户端的动态数据和协议状态。 */
bool xrtSshClientClear(xsshclient* pClient)
{
	if ( !xsshClientValid(pClient) ||
		((pClient->State != XSSH_CLIENT_CREATED) &&
		 (pClient->State != XSSH_CLIENT_CLOSED)) ) {
		return false;
	}
	if ( !xrtSshSessionStreamClear(&pClient->Stream) ) {
		return false;
	}
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		__xrtSshClientFutureClear(pClient);
	#endif
	if ( pClient->ResourcesReady ) {
		xrtNetBufClear(&pClient->Control);
		xrtSshChannelsClear(&pClient->Channels);
	}
	xrtFree(pClient->GlobalReplyTokens);
	xrtErrorFree(pClient->TerminalError);
	xrtSshClientCoreClear(&pClient->Core);
	memset(pClient, 0, sizeof(*pClient));
	return true;
}



/* 复用 SessionStream 唯一网络适配器。 */
const xnetstreamevents* xrtSshClientNetEvents(void)
{
	return xrtSshSessionStreamNetEvents();
}



/* 返回 NetEvents 要求的稳定 SessionStream 地址。 */
ptr xrtSshClientNetData(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? (ptr)&pClient->Stream : NULL;
}



/* 把已连接 Stream 附着到组合客户端。 */
bool xrtSshClientAttach(xsshclient* pClient, xnetstream* pStream)
{
	return xsshClientValid(pClient) &&
		(pClient->State == XSSH_CLIENT_CREATED) &&
		xrtSshSessionStreamAttach(&pClient->Stream, pStream);
}



/* 返回公开客户端状态。 */
xsshclientstate xrtSshClientState(const xsshclient* pClient)
{
	return xsshClientValid(pClient) ? pClient->State : XSSH_CLIENT_INVALID;
}



/* 返回客户端是否位于其唯一 Stream 所属 Worker。 */
bool xrtSshClientIsCurrent(const xsshclient* pClient)
{
	return xsshClientCurrent(pClient);
}



/* 返回完整 Stream 驱动。 */
xsshsessionstream* xrtSshClientStream(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? &pClient->Stream : NULL;
}



/* 返回底层 TCP 会话。 */
xsshsessiontcp* xrtSshClientSession(xsshclient* pClient)
{
	return xsshClientValid(pClient) ?
		xrtSshSessionStreamSession(&pClient->Stream) : NULL;
}



/* 返回动态 packet Reader。 */
xsshsessionreader* xrtSshClientReader(xsshclient* pClient)
{
	return xsshClientValid(pClient) ?
		xrtSshSessionStreamReader(&pClient->Stream) : NULL;
}



/* 返回动态 channel 所有者。 */
xsshchannels* xrtSshClientChannels(xsshclient* pClient)
{
	return xsshClientValid(pClient) && pClient->ResourcesReady ?
		&pClient->Channels : NULL;
}



/* 查询 channel 地址与本地编号是否仍映射到当前客户端。 */
bool xrtSshClientOwnsChannel(
	const xsshclient* pClient,
	const xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel);
}



/* 暂存接受当前 peer channel open 的决定。 */
xsshcode xrtSshClientChannelAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
)
{
	if ( !xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			ppChannel,
			sizeof(*ppChannel)
		) || xrtMemRangesOverlap(
			pOpen,
			sizeof(*pOpen),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*ppChannel = NULL;
	return xsshClientOpenStage(
		pClient,
		pOpen,
		XSSH_CLIENT_CHANNEL_ACCEPT,
		0u,
		ppChannel
	);
}



/* 暂存拒绝当前 peer channel open 的决定。 */
xsshcode xrtSshClientChannelReject(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	uint32 iReason
)
{
	return xsshClientOpenStage(
		pClient,
		pOpen,
		XSSH_CLIENT_CHANNEL_REJECT,
		iReason,
		NULL
	);
}



/* 返回连接级 request reply FIFO。 */
xsshreplyqueue* xrtSshClientGlobalReplies(xsshclient* pClient)
{
	return xsshClientValid(pClient) ? &pClient->GlobalReplies : NULL;
}



/* 按需扩展连接级 token ring，失败时保留原队列。 */
xsshcode xrtSshClientGlobalReplyReserve(
	xsshclient* pClient,
	size_t iCapacity
)
{
	uint64* pTokens;
	size_t iNewCapacity;
	xsshcode Code;

	if ( !xsshClientValid(pClient) ||
		((pClient->State != XSSH_CLIENT_CREATED) &&
		 !xsshClientCurrent(pClient)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( iCapacity > pClient->Config.GlobalReplyLimit ) {
		xrtSetErrorKind(XERR_RANGE);
		return XSSH_ERROR_SPACE;
	}
	if ( iCapacity <= pClient->GlobalReplyCapacity ) {
		return XSSH_OK;
	}
	iNewCapacity = pClient->GlobalReplyCapacity != 0u ?
		pClient->GlobalReplyCapacity : 4u;
	while ( iNewCapacity < iCapacity ) {
		if ( iNewCapacity > (SIZE_MAX / 2u) ) {
			iNewCapacity = iCapacity;
			break;
		}
		iNewCapacity *= 2u;
	}
	if ( iNewCapacity > pClient->Config.GlobalReplyLimit ) {
		iNewCapacity = pClient->Config.GlobalReplyLimit;
	}
	if ( iNewCapacity > (SIZE_MAX / sizeof(uint64)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pTokens = (uint64*)xrtMalloc(iNewCapacity * sizeof(uint64));
	if ( pTokens == NULL ) {
		return XSSH_ERROR_SPACE;
	}
	Code = xrtSshReplyQueueRebind(
		&pClient->GlobalReplies,
		pTokens,
		iNewCapacity
	);
	if ( Code != XSSH_OK ) {
		xrtFree(pTokens);
		return Code;
	}
	xrtFree(pClient->GlobalReplyTokens);
	pClient->GlobalReplyTokens = pTokens;
	pClient->GlobalReplyCapacity = iNewCapacity;
	return XSSH_OK;
}



/* 在 Worker 中重试外部凭据动作并继续驱动线路。 */
xsshcode xrtSshClientContinue(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	pClient->AuthNotified = false;
	Code = xsshClientAdvance(pClient);
	return Code == XSSH_OK ? xsshClientDrive(pClient) : Code;
}



/* 接受延迟主机密钥并继续 KEX。 */
xsshcode xrtSshClientHostKeyAccept(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreHostKeyAccept(
		&pClient->Core,
		&pClient->Stream.Session
	);
	pClient->HostNotified = false;
	return Code == XSSH_OK ? xsshClientDrive(pClient) : Code;
}



/* 拒绝延迟主机密钥并异常终止连接。 */
xsshcode xrtSshClientHostKeyReject(xsshclient* pClient)
{
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_HANDSHAKE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshClientCoreHostKeyReject(
		&pClient->Core,
		&pClient->Stream.Session
	);
	if ( Code == XSSH_OK ) {
		pClient->State = XSSH_CLIENT_CLOSING;
		(void)xrtSshSessionStreamAbort(&pClient->Stream);
	}
	return Code;
}



/* 提交用户保留且已经完成内部准备的 packet。 */
xsshcode xrtSshClientPacketAccept(xsshclient* pClient)
{
	const xsshchannelopen* pOpen;
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) || pClient->ReceiveRetry ) {
		return XSSH_ERROR_STATE;
	}
	pOpen = xsshClientCurrentOpen(pClient);
	if ( (pOpen != NULL) &&
		(pClient->OpenDecision == XSSH_CLIENT_CHANNEL_NONE) ) {
		Code = xsshClientOpenStage(
			pClient,
			pOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xrtSshSessionStreamAccept(&pClient->Stream);
	return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
}



/* 回滚 staged DATA 并拒绝当前 packet。 */
xsshcode xrtSshClientPacketReject(xsshclient* pClient)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshClientCurrent(pClient) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pClient->ReceiveChannel != NULL) &&
		(pClient->ReceiveChannel->Io.Pending ==
		 XSSH_CHANNEL_IO_PENDING_RECEIVE) ) {
		Code = xrtSshChannelIoReceiveAbort(&pClient->ReceiveChannel->Io);
	}
	xsshClientPacketStageAbort(pClient);
	pClient->ReceiveRetry = false;
	pClient->ReceiveRetryOpen = false;
	if ( xrtSshSessionStreamReject(&pClient->Stream) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	return Code;
}



/* 在 OOM 恢复后重跑内部 DATA 准备和用户 Packet 决策。 */
xsshcode xrtSshClientPacketRetry(xsshclient* pClient)
{
	const xsshsessiontcppacket* pPacket;
	xsshsessionstreamdecision Decision;
	xsshcode Code;

	if ( !xsshClientCurrent(pClient) || !pClient->ReceiveRetry ) {
		return XSSH_ERROR_STATE;
	}
	pPacket = xrtSshSessionStreamPacket(&pClient->Stream);
	if ( pPacket == NULL ) {
		return XSSH_ERROR_STATE;
	}
	if ( pClient->ReceiveRetryOpen ) {
		const xsshchannelopen* pOpen =
			&pPacket->Session.Message.Connection.Message.ChannelOpen;

		Code = xsshClientOpenStage(
			pClient,
			pOpen,
			XSSH_CLIENT_CHANNEL_REJECT,
			XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE,
			NULL
		);

		if ( Code == XSSH_ERROR_SPACE ) {
			return Code;
		}
		pClient->ReceiveRetry = false;
		pClient->ReceiveRetryOpen = false;
		if ( Code != XSSH_OK ) {
			(void)xrtSshSessionStreamReject(&pClient->Stream);
			return Code;
		}
		Code = xrtSshSessionStreamAccept(&pClient->Stream);
		return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
	}
	pClient->ReceiveRetry = false;
	pClient->ReceiveRetryOpen = false;
	pClient->ReceiveChannel = NULL;
	Decision = xsshClientPacketDecide(pClient, pPacket);
	if ( Decision == XSSH_SESSION_STREAM_ACCEPT ) {
		Code = xrtSshSessionStreamAccept(&pClient->Stream);
		return Code == XSSH_NEED_MORE ? XSSH_OK : Code;
	}
	if ( Decision == XSSH_SESSION_STREAM_HOLD ) {
		return XSSH_OK;
	}
	(void)xrtSshSessionStreamReject(&pClient->Stream);
	return XSSH_ERROR_STATE;
}



/* 直接编码并提交一个已有 payload。 */
xsshcode xrtSshClientSend(
	xsshclient* pClient,
	xbytesview Payload,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xsshsessionpacketkind Kind;
	xsshcode Code;
	bool bChannelIo;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pChannel != NULL) &&
		(!xsshClientChannelOwned(pClient, pChannel) ||
		 ((pReplies != NULL) &&
		  (pReplies != &pChannel->Replies))) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pChannel == NULL) && (pReplies != NULL) &&
		(pReplies != &pClient->GlobalReplies) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	bChannelIo = (pChannel != NULL) &&
		(pChannel->Io.Pending == XSSH_CHANNEL_IO_PENDING_SEND);
	if ( bChannelIo ) {
		if ( pClient->SendPending ) {
			return XSSH_ERROR_STATE;
		}
		pClient->SendChannel = pChannel;
		pClient->SendPending = true;
	}
	Code = xrtSshSessionTcpWritePrepare(
		&pClient->Stream.Session,
		Payload,
		pChannel != NULL ? &pChannel->Core : NULL,
		pReplies,
		iReplyToken,
		xrtClock() / 1000u,
		&Kind
	);
	if ( Code != XSSH_OK ) {
		if ( bChannelIo ) {
			(void)xrtSshChannelIoSendAbort(&pChannel->Io);
			pClient->SendChannel = NULL;
			pClient->SendPending = false;
		}
		return Code;
	}
	Code = xsshClientDrive(pClient);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionTcpWriteAbort(&pClient->Stream.Session);
		if ( pClient->SendPending ) {
			(void)xrtSshChannelIoSendAbort(&pChannel->Io);
			pClient->SendChannel = NULL;
			pClient->SendPending = false;
		}
	}
	return Code;
}



/* 在 Worker 缓冲池中按需扩展连续 scratch 并发送构建结果。 */
xsshcode xrtSshClientBuild(
	xsshclient* pClient,
	xsshclientbuildproc pBuild,
	ptr pBuildData,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
)
{
	xnetwspan Span;
	xsshwriter Writer;
	xsshcode Code;
	size_t iCapacity;

	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ||
		(pBuild == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	for ( ;; ) {
		if ( !xrtNetBufReserve(
			&pClient->Control,
			pClient->ControlTarget,
			&Span
		) ) {
			return XSSH_ERROR_SPACE;
		}
		iCapacity = Span.Size < pClient->Config.ControlLimit ?
			Span.Size : pClient->Config.ControlLimit;
		xrtSecureZero(Span.Data, Span.Size);
		if ( !xrtSshWriterInit(&Writer, Span.Data, iCapacity) ) {
			(void)xrtNetBufCancel(&pClient->Control);
			return XSSH_ERROR_STATE;
		}
		Code = pBuild(&Writer, pBuildData);
		if ( Code != XSSH_ERROR_SPACE ) {
			break;
		}
		xrtSecureZero(Span.Data, Span.Size);
		if ( !xrtNetBufCancel(&pClient->Control) ||
			(iCapacity >= pClient->Config.ControlLimit) ) {
			return XSSH_ERROR_SPACE;
		}
		if ( pClient->ControlTarget <=
			(pClient->Config.ControlLimit / 2u) ) {
			pClient->ControlTarget *= 2u;
		} else {
			pClient->ControlTarget = pClient->Config.ControlLimit;
		}
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshClientSend(
			pClient,
			(xbytesview){ Span.Data, Writer.Size },
			pChannel,
			pReplies,
			iReplyToken
		);
	}
	xrtSecureZero(Span.Data, Span.Size);
	if ( !xrtNetBufCancel(&pClient->Control) ) {
		return XSSH_ERROR_STATE;
	}
	return Code;
}



/* 创建 channel 并用调用方类型构建器发送唯一 open。 */
xsshcode xrtSshClientChannelOpen(
	xsshclient* pClient,
	xsshclientchannelopenproc pOpen,
	ptr pOpenData,
	xsshchannel** ppChannel
)
{
	xsshclientopenbuild Build;
	xsshchannel* pChannel;
	xsshchannels* pChannels;
	xsshcode Code;
	uint32 iLocal;

	if ( (pOpen == NULL) ||
		!xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pClient,
			sizeof(*pClient),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*ppChannel = NULL;
	if ( !xsshClientCurrent(pClient) ||
		(pClient->State != XSSH_CLIENT_READY) ) {
		return XSSH_ERROR_STATE;
	}
	pChannels = &pClient->Channels;
	Code = xrtSshChannelsOpen(pChannels, &pChannel);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iLocal = pChannel->Core.Local;
	Build.Open = pOpen;
	Build.Channel = pChannel;
	Build.UserData = pOpenData;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientChannelOpenBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( Code != XSSH_OK ) {
		if ( !xrtSshChannelsDiscard(pChannels, iLocal) ) {
			return XSSH_ERROR_STATE;
		}
		return Code;
	}
	*ppChannel = pChannel;
	return XSSH_OK;
}



/* 把一条 channel 数据流的连续队首提交给客户端。 */
xsshcode xrtSshClientChannelFlush(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
)
{
	xsshclientflushbuild Build;
	xsshcode Code;

	if ( !xsshClientChannelOwned(pClient, pChannel) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Build.Channel = pChannel;
	Build.Stream = Stream;
	Code = xrtSshClientBuild(
		pClient,
		xsshClientChannelFlushBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
	if ( (Code != XSSH_OK) &&
		(pChannel->Io.Pending == XSSH_CHANNEL_IO_PENDING_SEND) ) {
		(void)xrtSshChannelIoSendAbort(&pChannel->Io);
	}
	return Code;
}



/* 返还当前 channel 已消费的接收额度。 */
xsshcode xrtSshClientChannelAdjust(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	if ( !xsshClientChannelOwned(pClient, pChannel) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtSshChannelCoreAdjustReady(&pChannel->Core) ) {
		return XSSH_NEED_MORE;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientChannelAdjustBuild,
		pChannel,
		pChannel,
		NULL,
		0u
	);
}



/* 半关闭 channel 的本端写方向。 */
xsshcode xrtSshClientChannelEof(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel) ?
		xrtSshClientBuild(
			pClient,
			xsshClientChannelEofBuild,
			pChannel,
			pChannel,
			NULL,
			0u
		) : XSSH_ERROR_ARGUMENT;
}



/* 发起或响应 channel 双向关闭。 */
xsshcode xrtSshClientChannelClose(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return xsshClientChannelOwned(pClient, pChannel) ?
		xrtSshClientBuild(
			pClient,
			xsshClientChannelCloseBuild,
			pChannel,
			pChannel,
			NULL,
			0u
		) : XSSH_ERROR_ARGUMENT;
}



/* 委托 SessionStream 回滚并异常关闭。 */
bool xrtSshClientAbort(xsshclient* pClient)
{
	if ( !xsshClientCurrent(pClient) ||
		(pClient->State >= XSSH_CLIENT_CLOSING) ) {
		return false;
	}
	pClient->State = XSSH_CLIENT_CLOSING;
	return xrtSshSessionStreamAbort(&pClient->Stream);
}

#endif
