#include <xrt/ssh_client_forward.h>



#if defined(XSSH_FEATURE_CLIENT_FORWARD)

/* Direct channel 构建上下文只在同步 open 构建期间借用地址。 */
typedef struct xsshclientdirectbuild {
	xbytesview Host;
	xbytesview Originator;
	uint32 Port;
	uint32 OriginatorPort;
} xsshclientdirectbuild;



/* 全局 forwarding 构建上下文区分申请和取消。 */
typedef struct xsshclientforwardbuild {
	xbytesview Address;
	uint32 Port;
	bool Cancel;
} xsshclientforwardbuild;



/* 写出 direct-tcpip open，并沿用动态 channel 的窗口配置。 */
static xsshcode xsshClientDirectBuild(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pData
)
{
	xsshclientdirectbuild* pBuild = (xsshclientdirectbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ||
		!xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		(xrtSshChannelCorePhase(pChannel) !=
		 XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshDirectTcpipOpenWrite(
		pWriter,
		pChannel->Local,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		pBuild->Host,
		pBuild->Port,
		pBuild->Originator,
		pBuild->OriginatorPort
	);
}



/* 写出要求回复的 forwarding 全局请求。 */
static xsshcode xsshClientForwardBuild(
	xsshwriter* pWriter,
	ptr pData
)
{
	xsshclientforwardbuild* pBuild = (xsshclientforwardbuild*)pData;

	if ( !xrtMemRangeValid(pBuild, sizeof(*pBuild)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return pBuild->Cancel ? xrtSshTcpipForwardCancelWrite(
		pWriter,
		pBuild->Address,
		pBuild->Port
	) : xrtSshTcpipForwardWrite(
		pWriter,
		pBuild->Address,
		pBuild->Port
	);
}



/* 为全局请求预留 reply token 后提交唯一事务。 */
static xsshcode xsshClientForwardSend(
	xsshclient* pClient,
	xsshclientforwardbuild* pBuild,
	uint64 iReplyToken
)
{
	xsshreplyqueue* pReplies;
	size_t iCount;
	xsshcode Code;

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtMemRangeValid(pBuild, sizeof(*pBuild)) ) {
		return XSSH_ERROR_STATE;
	}
	pReplies = xrtSshClientGlobalReplies(pClient);
	iCount = xrtSshReplyQueueCount(pReplies);
	if ( iCount == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshClientGlobalReplyReserve(pClient, iCount + 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientForwardBuild,
		pBuild,
		NULL,
		NULL,
		iReplyToken
	);
}



/* 打开一个动态 direct-tcpip channel。 */
xsshcode xrtSshClientDirectTcpipOpen(
	xsshclient* pClient,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort,
	xsshchannel** ppChannel
)
{
	xsshclientdirectbuild Build = {
		Host,
		Originator,
		iPort,
		iOriginatorPort
	};

	return xrtSshClientChannelOpen(
		pClient,
		xsshClientDirectBuild,
		&Build,
		ppChannel
	);
}



/* 解析 peer forwarding 字段并暂存读提交后的 confirmation。 */
xsshcode xrtSshClientForwardedTcpipAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip,
	xsshchannel** ppChannel
)
{
	xsshtcpipopen Tcpip;
	xsshcode Code;

	if ( !xrtMemRangeValid(pTcpip, sizeof(*pTcpip)) ||
		!xrtMemRangeValid(ppChannel, sizeof(*ppChannel)) ||
		xrtMemRangesOverlap(
			pTcpip,
			sizeof(*pTcpip),
			ppChannel,
			sizeof(*ppChannel)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshForwardedTcpipOpenRead(pOpen, &Tcpip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshClientChannelAccept(pClient, pOpen, ppChannel);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pTcpip = Tcpip;
	return XSSH_OK;
}



/* 请求服务端创建 remote forwarding 监听。 */
xsshcode xrtSshClientTcpipForward(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
)
{
	xsshclientforwardbuild Build = { Address, iPort, false };

	return xsshClientForwardSend(pClient, &Build, iReplyToken);
}



/* 请求服务端撤销 remote forwarding 监听。 */
xsshcode xrtSshClientTcpipForwardCancel(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
)
{
	xsshclientforwardbuild Build = { Address, iPort, true };

	return xsshClientForwardSend(pClient, &Build, iReplyToken);
}

#endif
