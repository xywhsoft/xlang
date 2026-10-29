#include <xrt/ssh_client_session.h>



#if defined(XSSH_FEATURE_CLIENT_SESSION)

/* 单一构建上下文覆盖 session request 的全部经典类型。 */
typedef enum xsshclientsessionbuildkind {
	XSSH_CLIENT_SESSION_BUILD_REQUEST = 0,
	XSSH_CLIENT_SESSION_BUILD_ENV = 1,
	XSSH_CLIENT_SESSION_BUILD_SHELL = 2,
	XSSH_CLIENT_SESSION_BUILD_EXEC = 3,
	XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM = 4,
	XSSH_CLIENT_SESSION_BUILD_SIGNAL = 5,
	XSSH_CLIENT_SESSION_BUILD_BREAK = 6
} xsshclientsessionbuildkind;



/* 构建期间只借用输入视图，发送返回后不保留调用方文本。 */
typedef struct xsshclientsessionbuild {
	xsshchannel* Channel;
	xstrview Type;
	xbytesview First;
	xbytesview Second;
	uint32 Value;
	bool WantReply;
	xsshclientsessionbuildkind Kind;
} xsshclientsessionbuild;



/* 判断调用发生在 READY 客户端 Worker 且 channel 归属正确。 */
static bool xsshClientSessionReady(
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	return (xrtSshClientState(pClient) == XSSH_CLIENT_READY) &&
		xrtSshClientIsCurrent(pClient) &&
		xrtSshClientOwnsChannel(pClient, pChannel);
}



/* 为 want-reply 请求预留一个 FIFO 位置，失败不改变队列内容。 */
static xsshcode xsshClientSessionReplyReserve(
	xsshchannel* pChannel,
	bool bWantReply
)
{
	size_t iCount;

	if ( !bWantReply ) {
		return XSSH_OK;
	}
	iCount = xrtSshReplyQueueCount(&pChannel->Replies);
	if ( iCount == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	return xrtSshChannelReplyReserve(pChannel, iCount + 1u);
}



/* 构建固定 session channel open。 */
static xsshcode xsshClientSessionOpenBuild(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pData
)
{
	(void)pData;
	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		(xrtSshChannelCorePhase(pChannel) !=
		 XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_SESSION),
		pChannel->Local,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		(xbytesview){ NULL, 0u }
	);
}



/* 根据类型写出一个 session request，并统一使用远端 channel id。 */
static xsshcode xsshClientSessionRequestBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientsessionbuild* pBuild = (xsshclientsessionbuild*)pData;
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
	(void)iLocal;
	switch ( pBuild->Kind ) {
		case XSSH_CLIENT_SESSION_BUILD_REQUEST:
			return xrtSshChannelRequestWrite(
				pWriter,
				iRemote,
				pBuild->Type,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_ENV:
			return xrtSshChannelEnvWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First,
				pBuild->Second
			);
		case XSSH_CLIENT_SESSION_BUILD_SHELL:
			return xrtSshChannelShellWrite(
				pWriter,
				iRemote,
				pBuild->WantReply
			);
		case XSSH_CLIENT_SESSION_BUILD_EXEC:
			return xrtSshChannelExecWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM:
			return xrtSshChannelSubsystemWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->First
			);
		case XSSH_CLIENT_SESSION_BUILD_SIGNAL:
			return xrtSshChannelSignalWrite(
				pWriter,
				iRemote,
				pBuild->Type
			);
		case XSSH_CLIENT_SESSION_BUILD_BREAK:
			return xrtSshChannelBreakWrite(
				pWriter,
				iRemote,
				pBuild->WantReply,
				pBuild->Value
			);
		default:
			return XSSH_ERROR_ARGUMENT;
	}
}



/* 校验并发送一个已经填充的 session request。 */
static xsshcode xsshClientSessionSend(
	xsshclient* pClient,
	xsshclientsessionbuild* pBuild,
	uint64 iReplyToken
)
{
	xsshreplyqueue* pReplies;
	xsshcode Code;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xsshClientSessionReady(pClient, pBuild->Channel) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshClientSessionReplyReserve(
		pBuild->Channel,
		pBuild->WantReply
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pReplies = pBuild->WantReply ? &pBuild->Channel->Replies : NULL;
	return xrtSshClientBuild(
		pClient,
		xsshClientSessionRequestBuild,
		pBuild,
		pBuild->Channel,
		pReplies,
		iReplyToken
	);
}



/* 打开不带类型专用字段的 session channel。 */
xsshcode xrtSshClientSessionOpen(
	xsshclient* pClient,
	xsshchannel** ppChannel
)
{
	return xrtSshClientChannelOpen(
		pClient,
		xsshClientSessionOpenBuild,
		NULL,
		ppChannel
	);
}



/* 发送调用方提供类型与已编码字段的扩展请求。 */
xsshcode xrtSshClientSessionRequest(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Type,
	xbytesview Fields,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		Type,
		Fields,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_REQUEST
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 env request。 */
xsshcode xrtSshClientSessionEnv(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Name,
	xbytesview Value,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Name,
		Value,
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_ENV
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 shell request。 */
xsshcode xrtSshClientSessionShell(
	xsshclient* pClient,
	xsshchannel* pChannel,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_SHELL
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 exec request。 */
xsshcode xrtSshClientSessionExec(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Command,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Command,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_EXEC
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送 subsystem request。 */
xsshcode xrtSshClientSessionSubsystem(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Subsystem,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		Subsystem,
		{ NULL, 0u },
		0u,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_SUBSYSTEM
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}



/* 发送不要求回复的 signal request。 */
xsshcode xrtSshClientSessionSignal(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Signal
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		Signal,
		{ NULL, 0u },
		{ NULL, 0u },
		0u,
		false,
		XSSH_CLIENT_SESSION_BUILD_SIGNAL
	};

	return xsshClientSessionSend(pClient, &Build, 0u);
}



/* 发送 break request。 */
xsshcode xrtSshClientSessionBreak(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iLengthMs,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientsessionbuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		{ NULL, 0u },
		iLengthMs,
		bWantReply,
		XSSH_CLIENT_SESSION_BUILD_BREAK
	};

	return xsshClientSessionSend(pClient, &Build, iReplyToken);
}

#endif
