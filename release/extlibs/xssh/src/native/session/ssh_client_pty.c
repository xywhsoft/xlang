#include <xrt/ssh_client_pty.h>



#if defined(XSSH_FEATURE_CLIENT_PTY)

/* PTY 与 resize 共用尺寸字段，只有 PTY 借用 Terminal 和 Modes。 */
typedef struct xsshclientptybuild {
	xsshchannel* Channel;
	xbytesview Terminal;
	xbytesview Modes;
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
	bool WantReply;
	bool Resize;
} xsshclientptybuild;



/* 写出 PTY 或 window-change，并统一使用远端 channel id。 */
static xsshcode xsshClientPtyBuild(xsshwriter* pWriter, ptr pData)
{
	xsshclientptybuild* pBuild = (xsshclientptybuild*)pData;
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
	if ( pBuild->Resize ) {
		return xrtSshChannelWindowChangeWrite(
			pWriter,
			iRemote,
			pBuild->Columns,
			pBuild->Rows,
			pBuild->PixelWidth,
			pBuild->PixelHeight
		);
	}
	return xrtSshChannelPtyWrite(
		pWriter,
		iRemote,
		pBuild->WantReply,
		pBuild->Terminal,
		pBuild->Columns,
		pBuild->Rows,
		pBuild->PixelWidth,
		pBuild->PixelHeight,
		pBuild->Modes
	);
}



/* 发送 PTY 请求，并按需预留 channel reply token。 */
xsshcode xrtSshClientSessionPty(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes,
	bool bWantReply,
	uint64 iReplyToken
)
{
	xsshclientptybuild Build = {
		pChannel,
		Terminal,
		Modes,
		iColumns,
		iRows,
		iPixelWidth,
		iPixelHeight,
		bWantReply,
		false
	};
	xsshreplyqueue* pReplies = NULL;
	size_t iCount;
	xsshcode Code;

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	if ( bWantReply ) {
		iCount = xrtSshReplyQueueCount(&pChannel->Replies);
		if ( iCount == SIZE_MAX ) {
			return XSSH_ERROR_OVERFLOW;
		}
		Code = xrtSshChannelReplyReserve(pChannel, iCount + 1u);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		pReplies = &pChannel->Replies;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientPtyBuild,
		&Build,
		pChannel,
		pReplies,
		iReplyToken
	);
}



/* 发送 window-change 通知。 */
xsshcode xrtSshClientSessionResize(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
)
{
	xsshclientptybuild Build = {
		pChannel,
		{ NULL, 0u },
		{ NULL, 0u },
		iColumns,
		iRows,
		iPixelWidth,
		iPixelHeight,
		false,
		true
	};

	if ( (xrtSshClientState(pClient) != XSSH_CLIENT_READY) ||
		!xrtSshClientIsCurrent(pClient) ||
		!xrtSshClientOwnsChannel(pClient, pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshClientBuild(
		pClient,
		xsshClientPtyBuild,
		&Build,
		pChannel,
		NULL,
		0u
	);
}

#endif
