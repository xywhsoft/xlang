#include <string.h>

#include <xrt/ssh_session_tcp.h>



#if defined(XSSH_FEATURE_SESSION_TCP)

#define XSSH_SESSION_TCP_GUARD UINT32_C(0x53535450)



/* 校验组合对象以及两个子层的角色一致性。 */
static bool xsshSessionTcpValid(const xsshsessiontcp* pSession)
{
	const xsshtransporttcp* pTransport;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		(pSession->Guard != XSSH_SESSION_TCP_GUARD) ||
		!pSession->Session.Initialized ) {
		return false;
	}
	pTransport = xrtSshTransportTcpCoreConst(&pSession->Transport) != NULL ?
		&pSession->Transport : NULL;
	if ( (pTransport == NULL) ||
		(pTransport->Core.State.Role != pSession->Session.Role) ) {
		return false;
	}
	return true;
}



/* 清空只在未决 packet 读取期间有效的外部缓冲借用。 */
static void xsshSessionTcpReadClear(xsshsessiontcp* pSession)
{
	memset(&pSession->ReadPacket, 0, sizeof(pSession->ReadPacket));
	pSession->ReadVersion = (xstrview){ NULL, 0u };
	pSession->ReadPlain = NULL;
	pSession->ReadPlainCapacity = 0u;
}



/* 把无法恢复的跨层提交错误发布到 XRT 并终止整条链路。 */
static void xsshSessionTcpFail(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	xsshcode Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(
		Code == XSSH_ERROR_PROTOCOL ? XERR_PROTOCOL : XERR_INTERNAL,
		"xrt.ssh",
		(int32)Code,
		sMessage
	);
	xrtSshSessionCoreFail(&pSession->Session);
	xrtSshTransportCoreClose(&pSession->Transport.Core);
	xsshSessionTcpReadClear(pSession);
	if ( pStream != NULL ) {
		(void)xrtNetStreamAbort(pStream);
	}
}



/* 校验 packet 读取的外部对象，避免成功后写回破坏未消费线路数据。 */
static bool xsshSessionTcpReadArguments(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	xsshsessiontcppacket* pPacket
)
{
	if ( (pInput == NULL) ||
		!xrtMemRangeValid(pPlain, iPlainCapacity) ||
		!xrtMemRangeValid(pHostKeyStorage, iHostKeyCapacity) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		((pHostKeySize != NULL) &&
		 !xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize))) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pInput,
		sizeof(*pInput)
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pPlain,
		iPlainCapacity
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pHostKeyStorage,
		iHostKeyCapacity
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pInput,
		sizeof(*pInput),
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pPlain,
		iPlainCapacity,
		pPacket,
		sizeof(*pPacket)
	) || xrtMemRangesOverlap(
		pHostKeyStorage,
		iHostKeyCapacity,
		pPacket,
		sizeof(*pPacket)
	) ) {
		return false;
	}
	if ( (pHostKeySize != NULL) &&
		(xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pPlain,
			iPlainCapacity,
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize,
			sizeof(*pHostKeySize)
		) || xrtMemRangesOverlap(
			pPacket,
			sizeof(*pPacket),
			pHostKeySize,
			sizeof(*pHostKeySize)
		)) ) {
		return false;
	}
	return true;
}



/* 默认配置直接复用 transport 的稳定预算。 */
bool xrtSshSessionTcpConfigInit(
	xsshsessiontcpconfig* pConfig,
	xsshrole Role
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	return xrtSshTransportTcpConfigInit(&pConfig->Transport, Role);
}



/* 两个子对象先在局部结构中完整初始化，再一次发布。 */
bool xrtSshSessionTcpInit(
	xsshsessiontcp* pSession,
	xnetbufpool* pPool,
	const xsshsessiontcpconfig* pConfig,
	uint64 iNowMs
)
{
	xsshsessiontcp Session;
	xsshsessiontcpconfig Config;
	size_t iReplyBytes = 0u;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pConfig,
			sizeof(*pConfig)
		) ) {
		return false;
	}
	Config = *pConfig;
	if ( Config.GlobalReplies != NULL ) {
		if ( !xrtMemRangeValid(
			Config.GlobalReplies,
			sizeof(*Config.GlobalReplies)
		) || (Config.GlobalReplies->Capacity >
			(SIZE_MAX / sizeof(uint64))) ) {
			return false;
		}
		iReplyBytes = Config.GlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Config.GlobalReplies,
			sizeof(*Config.GlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Config.GlobalReplies->Tokens,
			iReplyBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	if ( !xrtSshTransportTcpInit(
		&Session.Transport,
		pPool,
		&Config.Transport,
		iNowMs
	) || !xrtSshSessionCoreInit(
		&Session.Session,
		pPool,
		Config.Transport.Role,
		Config.ChannelResolve,
		Config.ChannelUserData,
		Config.GlobalReplies
	) ) {
		xrtSshSessionCoreClear(&Session.Session);
		xrtSshTransportTcpClear(&Session.Transport);
		return false;
	}
	Session.Guard = XSSH_SESSION_TCP_GUARD;
	xrtSecureZero(pSession, sizeof(*pSession));
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return true;
}



/* 协议层先释放 transcript，transport 随后回滚 packet 并清除 cipher。 */
void xrtSshSessionTcpClear(xsshsessiontcp* pSession)
{
	if ( pSession == NULL ) {
		return;
	}
	if ( xsshSessionTcpValid(pSession) ) {
		xrtSshSessionCoreClear(&pSession->Session);
		xrtSshTransportTcpClear(&pSession->Transport);
	}
	xrtSecureZero(pSession, sizeof(*pSession));
}



/* 返回组合对象持有的 TCP transport。 */
xsshtransporttcp* xrtSshSessionTcpTransport(xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Transport : NULL;
}



/* 返回只读 TCP transport。 */
const xsshtransporttcp* xrtSshSessionTcpTransportConst(
	const xsshsessiontcp* pSession
)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Transport : NULL;
}



/* 返回组合对象持有的连接级协议核心。 */
xsshsessioncore* xrtSshSessionTcpCore(xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Session : NULL;
}



/* 返回只读连接级协议核心。 */
const xsshsessioncore* xrtSshSessionTcpCoreConst(
	const xsshsessiontcp* pSession
)
{
	return xsshSessionTcpValid(pSession) ? &pSession->Session : NULL;
}



/* 连接阶段统一从协议核心和同一 transport 推导。 */
xsshsessionphase xrtSshSessionTcpPhase(const xsshsessiontcp* pSession)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_SESSION_FAILED;
	}
	return xrtSshSessionCorePhase(
		&pSession->Session,
		&pSession->Transport.Core
	);
}



/* TCP 组合对象直接复用连接级统一动作推导。 */
xsshsessionaction xrtSshSessionTcpAction(const xsshsessiontcp* pSession)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_SESSION_ACTION_FAILED;
	}
	return xrtSshSessionCoreAction(
		&pSession->Session,
		&pSession->Transport.Core
	);
}



/* 确定性 KEX 便利入口只消除两个子对象访问样板。 */
xsshcode xrtSshSessionTcpKexBeginWithPrivate(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		!xrtMemRangeValid(PrivateKey.Data, PrivateKey.Size) ||
		xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		ServerHostKey.Data,
		ServerHostKey.Size
	) || xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		PrivateKey.Data,
		PrivateKey.Size
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreKexBeginWithPrivate(
		&pSession->Session,
		&pSession->Transport.Core,
		ServerHostKey,
		PrivateKey
	);
}



/* 认证便利入口保留调用方策略和时钟所有权。 */
xsshcode xrtSshSessionTcpAuthBegin(
	xsshsessiontcp* pSession,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pPolicy != NULL) &&
		(!xrtMemRangeValid(pPolicy, sizeof(*pPolicy)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPolicy,
			sizeof(*pPolicy)
		)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshSessionCoreAuthBegin(
		&pSession->Session,
		&pSession->Transport.Core,
		pPolicy,
		iNowMs
	);
}



/* 版本先进入 transcript 暂存，再准备可重试的动态线路输出。 */
xsshcode xrtSshSessionTcpIdentificationWritePrepare(
	xsshsessiontcp* pSession,
	xstrview Version
)
{
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Version.Data, Version.Size) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Version.Data,
			Version.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		XSSH_TRANSPORT_LOCAL,
		Version
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshTransportTcpIdentificationPrepare(
		&pSession->Transport,
		Version
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreVersionAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	}
	return Code;
}



/* 协议候选、线路编码和事务绑定在一次调用中完整闭合。 */
xsshcode xrtSshSessionTcpWritePrepareWithPadding(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	xsshpaddingproc pPadding,
	ptr pPaddingData,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
)
{
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pKind, sizeof(*pKind)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pKind,
			sizeof(*pKind)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshSessionCoreWritePrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		Payload,
		pChannel,
		pReplies,
		iReplyToken,
		iNowMs,
		pKind
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshTransportTcpWritePrepareWithPadding(
		&pSession->Transport,
		Payload,
		pPadding,
		pPaddingData,
		iNowMs
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
		return Code;
	}
	Code = xrtSshSessionCoreWriteBind(
		&pSession->Session,
		&pSession->Transport.Core,
		Payload
	);
	if ( Code != XSSH_OK ) {
		(void)xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
		(void)xrtSshTransportTcpWriteAbort(&pSession->Transport);
	}
	return Code;
}



/* Stream 接管后 transport 先提交，协议层再发布相同事务。 */
xnetresult xrtSshSessionTcpWriteSubmit(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshtransporttcppending Pending;
	xnetresult Result;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"xrt.ssh",
			(int32)XSSH_ERROR_ARGUMENT,
			"invalid SSH TCP session write submission"
		);
		return XNET_RESULT_ERROR;
	}
	Pending = pSession->Transport.WritePending;
	if ( ((Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION) &&
		 ((pSession->Session.Kex.Pending !=
		   XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		  (pSession->Session.Kex.PendingDirection !=
		   XSSH_TRANSPORT_LOCAL))) ||
		((Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET) &&
		 ((pSession->Session.WritePending ==
		   XSSH_SESSION_PACKET_NONE) ||
		  !pSession->Session.WriteBound)) ||
		(Pending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			XSSH_ERROR_STATE,
			"SSH TCP session write transactions diverged"
		);
		return XNET_RESULT_ERROR;
	}
	Result = xrtSshTransportTcpWriteSubmit(
		&pSession->Transport,
		pStream,
		iNowMs,
		pDecision
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshSessionCoreVersionCommit(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		Code = xrtSshSessionCoreWriteCommit(
			&pSession->Session,
			&pSession->Transport.Core,
			iNowMs
		);
	}
	if ( Code != XSSH_OK ) {
		xsshSessionTcpFail(
			pSession,
			pStream,
			Code,
			"SSH session rejected TCP-accepted output"
		);
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}



/* 两个写事务按上层到 transport 的逆提交顺序回滚。 */
xsshcode xrtSshSessionTcpWriteAbort(xsshsessiontcp* pSession)
{
	xsshtransporttcppending Pending;
	xsshcode SessionCode;
	xsshcode TransportCode;

	if ( !xsshSessionTcpValid(pSession) ||
		(pSession->Transport.WritePending ==
		 XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Pending = pSession->Transport.WritePending;
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		SessionCode = xrtSshSessionCoreVersionAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		SessionCode = xrtSshSessionCoreWriteAbort(
			&pSession->Session,
			&pSession->Transport.Core
		);
	}
	TransportCode = xrtSshTransportTcpWriteAbort(&pSession->Transport);
	if ( (SessionCode != XSSH_OK) || (TransportCode != XSSH_OK) ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			SessionCode != XSSH_OK ? SessionCode : TransportCode,
			"SSH TCP session write rollback diverged"
		);
	}
	return SessionCode != XSSH_OK ? SessionCode : TransportCode;
}



/* 动态输出大小直接由 transport 持有。 */
size_t xrtSshSessionTcpWriteSize(const xsshsessiontcp* pSession)
{
	return xsshSessionTcpValid(pSession) ?
		xrtSshTransportTcpWriteSize(&pSession->Transport) : 0u;
}



/* transport 借出完整版本行后，协议层复制不含 CRLF 的 transcript。 */
xsshcode xrtSshSessionTcpIdentificationReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	xstrview* pVersion
)
{
	xstrview Version;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) || (pInput == NULL) ||
		!xrtMemRangeValid(pVersion, sizeof(*pVersion)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pInput,
			sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pVersion,
			sizeof(*pVersion)
		) || xrtMemRangesOverlap(
			pInput,
			sizeof(*pInput),
			pVersion,
			sizeof(*pVersion)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Transport.ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		if ( pSession->Transport.Input != pInput ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Version = pSession->ReadVersion;
	} else {
		Code = xrtSshTransportTcpIdentificationReadPrepare(
			&pSession->Transport,
			pInput,
			&Version
		);
		if ( Code != XSSH_OK ) {
			if ( (pSession->Transport.Core.State.Phase ==
				XSSH_TRANSPORT_CLOSING) ||
				(pSession->Transport.Core.State.Phase ==
				 XSSH_TRANSPORT_CLOSED) ) {
				xrtSshSessionCoreFail(&pSession->Session);
				xsshSessionTcpReadClear(pSession);
			}
			return Code;
		}
		pSession->ReadVersion = Version;
	}
	Code = xrtSshSessionCoreVersionPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		XSSH_TRANSPORT_PEER,
		Version
	);
	if ( Code == XSSH_OK ) {
		*pVersion = Version;
	} else if ( Code != XSSH_ERROR_SPACE ) {
		(void)xrtSshTransportTcpReadAbort(&pSession->Transport);
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH peer identification was rejected"
		);
	}
	return Code;
}



/* packet 探测不触碰协议核心事务。 */
xsshcode xrtSshSessionTcpReadInspect(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
)
{
	if ( !xsshSessionTcpValid(pSession) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNeed,
			sizeof(*pNeed)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshTransportTcpReadInspect(
		&pSession->Transport,
		pInput,
		pNeed
	);
}



/* transport 认证与会话路由共享同一个未消费线路前缀。 */
xsshcode xrtSshSessionTcpReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	uint64 iNowMs,
	xsshsessiontcppacket* pPacket
)
{
	xsshsessionpacket SessionPacket;
	xsshpacketview TransportPacket;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xsshSessionTcpReadArguments(
			pSession,
			pInput,
			pPlain,
			iPlainCapacity,
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize,
			pPacket
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Transport.ReadPending ==
		XSSH_TRANSPORT_TCP_PENDING_PACKET ) {
		if ( (pSession->Transport.Input != pInput) ||
			(pSession->ReadPlain != pPlain) ||
			(pSession->ReadPlainCapacity != iPlainCapacity) ||
			(pSession->Session.ReadPending !=
			 XSSH_SESSION_PACKET_NONE) ) {
			return XSSH_ERROR_STATE;
		}
		TransportPacket = pSession->ReadPacket;
	} else {
		Code = xrtSshTransportTcpReadPrepare(
			&pSession->Transport,
			pInput,
			&TransportPacket,
			pPlain,
			iPlainCapacity,
			iNowMs
		);
		if ( Code != XSSH_OK ) {
			if ( (pSession->Transport.Core.State.Phase ==
				XSSH_TRANSPORT_CLOSING) ||
				(pSession->Transport.Core.State.Phase ==
				 XSSH_TRANSPORT_CLOSED) ) {
				xrtSshSessionCoreFail(&pSession->Session);
				xsshSessionTcpReadClear(pSession);
			}
			return Code;
		}
		pSession->ReadPacket = TransportPacket;
		pSession->ReadPlain = pPlain;
		pSession->ReadPlainCapacity = iPlainCapacity;
	}
	Code = xrtSshSessionCoreReadPrepare(
		&pSession->Session,
		&pSession->Transport.Core,
		TransportPacket.Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		pHostKeySize,
		iNowMs,
		&SessionPacket
	);
	if ( Code == XSSH_OK ) {
		pPacket->Transport = TransportPacket;
		pPacket->Session = SessionPacket;
	} else if ( (Code != XSSH_ERROR_SPACE) &&
		(Code != XSSH_ERROR_ARGUMENT) ) {
		(void)xrtSshTransportTcpReadAbort(&pSession->Transport);
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH authenticated packet was rejected"
		);
	}
	return Code;
}



/* transport 消费成功后才发布版本或连接级协议状态。 */
xsshcode xrtSshSessionTcpReadCommit(
	xsshsessiontcp* pSession,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshtransporttcppending Pending;
	xsshcode Code;

	if ( !xsshSessionTcpValid(pSession) ||
		!xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Pending = pSession->Transport.ReadPending;
	if ( ((Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION) &&
		 ((pSession->Session.Kex.Pending !=
		   XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		  (pSession->Session.Kex.PendingDirection !=
		   XSSH_TRANSPORT_PEER))) ||
		((Pending == XSSH_TRANSPORT_TCP_PENDING_PACKET) &&
		 (pSession->Session.ReadPending ==
		  XSSH_SESSION_PACKET_NONE)) ||
		(Pending == XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportTcpReadCommit(
		&pSession->Transport,
		iNowMs,
		pDecision
	);
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(&pSession->Session);
		xsshSessionTcpReadClear(pSession);
		return Code;
	}
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		Code = xrtSshSessionCoreVersionCommit(
			&pSession->Session,
			&pSession->Transport.Core
		);
	} else {
		Code = xrtSshSessionCoreReadCommit(
			&pSession->Session,
			&pSession->Transport.Core,
			iNowMs
		);
	}
	xsshSessionTcpReadClear(pSession);
	if ( Code != XSSH_OK ) {
		xsshSessionTcpFail(
			pSession,
			NULL,
			Code,
			"SSH session rejected consumed TCP input"
		);
	}
	return Code;
}



/* 拒绝输入时两层都进入终止态，线路前缀只由 transport 消费一次。 */
xsshcode xrtSshSessionTcpReadAbort(xsshsessiontcp* pSession)
{
	xsshtransporttcppending Pending;
	xsshcode SessionCode = XSSH_OK;
	xsshcode TransportCode;

	if ( !xsshSessionTcpValid(pSession) ||
		(pSession->Transport.ReadPending ==
		 XSSH_TRANSPORT_TCP_PENDING_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Pending = pSession->Transport.ReadPending;
	if ( Pending == XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION ) {
		if ( pSession->Session.Kex.Pending ==
			XSSH_KEX_EXCHANGE_PENDING_VERSION ) {
			SessionCode = xrtSshSessionCoreVersionAbort(
				&pSession->Session,
				&pSession->Transport.Core
			);
		}
	} else if ( pSession->Session.ReadPending !=
		XSSH_SESSION_PACKET_NONE ) {
		SessionCode = xrtSshSessionCoreReadAbort(&pSession->Session);
	}
	TransportCode = xrtSshTransportTcpReadAbort(&pSession->Transport);
	xrtSshSessionCoreFail(&pSession->Session);
	xsshSessionTcpReadClear(pSession);
	return SessionCode != XSSH_OK ? SessionCode : TransportCode;
}

#endif
