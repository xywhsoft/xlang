#include <xrt/ssh_session_tcp_random.h>



#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM)

/* 随机 KEX 只组合已经分离的会话随机便利层。 */
xsshcode xrtSshSessionTcpKexBegin(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey
)
{
	xsshsessioncore* pCore = xrtSshSessionTcpCore(pSession);
	xsshtransporttcp* pTransport = xrtSshSessionTcpTransport(pSession);

	if ( (pCore == NULL) || (pTransport == NULL) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			ServerHostKey.Data,
			ServerHostKey.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreKexBegin(
		pCore,
		&pTransport->Core,
		ServerHostKey
	);
}



/* 安全随机 padding 不改变基础闭包的协议事务顺序。 */
xsshcode xrtSshSessionTcpWritePrepare(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
)
{
	return xrtSshSessionTcpWritePrepareWithPadding(
		pSession,
		Payload,
		pChannel,
		pReplies,
		iReplyToken,
		xrtSshSecurePadding,
		NULL,
		iNowMs,
		pKind
	);
}

#endif
