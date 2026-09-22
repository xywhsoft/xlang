#include <string.h>

#include <xrt/ssh_session_core.h>



#if defined(XSSH_FEATURE_SESSION_CORE)

#define XSSH_SESSION_CORE_GUARD UINT32_C(0x53455353)



/* 校验连接级对象的固定字段和未决事务枚举。 */
static bool xsshSessionCoreValid(const xsshsessioncore* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		pSession->Initialized &&
		(pSession->Guard == XSSH_SESSION_CORE_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->WritePending >= XSSH_SESSION_PACKET_NONE) &&
		(pSession->WritePending <= XSSH_SESSION_PACKET_EXTENSION) &&
		(pSession->ReadPending >= XSSH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending <= XSSH_SESSION_PACKET_EXTENSION);
}



/* 校验 transport 的角色和地址边界。 */
static bool xsshSessionCoreTransportValid(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role) &&
		(pCore->State.Phase >= XSSH_TRANSPORT_IDENTIFICATION) &&
		(pCore->State.Phase <= XSSH_TRANSPORT_CLOSED);
}



/* 判断任一子状态已经进入不可继续状态。 */
static bool xsshSessionCoreChildFailed(const xsshsessioncore* pSession)
{
	return (pSession->Kex.Phase == XSSH_KEX_EXCHANGE_FAILED) ||
		(pSession->Auth.Phase == XSSH_AUTH_SESSION_FAILED) ||
		pSession->Connection.Failed;
}



/* 清除本端未决事务描述。 */
static void xsshSessionCoreWriteClear(xsshsessioncore* pSession)
{
	pSession->WritePayload = (xbytesview){ NULL, 0u };
	pSession->WriteOrdinal = 0u;
	pSession->WritePending = XSSH_SESSION_PACKET_NONE;
	pSession->WriteMessage = 0u;
	pSession->WriteBound = false;
}



/* 清除对端未决事务描述。 */
static void xsshSessionCoreReadClear(xsshsessioncore* pSession)
{
	pSession->ReadOrdinal = 0u;
	pSession->ReadPending = XSSH_SESSION_PACKET_NONE;
	pSession->ReadMessage = 0u;
}



/* 将已知 transport 控制 payload 严格解析为借用视图。 */
static xsshcode xsshSessionCoreTransportPacket(
	xbytesview Payload,
	uint8 iMessage,
	xsshsessionpacket* pPacket,
	bool* pRecognized
)
{
	xsshcode Code;

	*pRecognized = true;
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		pPacket->Kind = XSSH_SESSION_PACKET_DISCONNECT;
		Code = xrtSshDisconnectRead(
			Payload,
			&pPacket->Message.Disconnect
		);
	} else if ( iMessage == XSSH_MSG_IGNORE ) {
		pPacket->Kind = XSSH_SESSION_PACKET_IGNORE;
		Code = xrtSshIgnoreRead(Payload, &pPacket->Message.Ignore);
	} else if ( iMessage == XSSH_MSG_UNIMPLEMENTED ) {
		pPacket->Kind = XSSH_SESSION_PACKET_UNIMPLEMENTED;
		Code = xrtSshUnimplementedRead(
			Payload,
			&pPacket->Message.UnimplementedSequence
		);
	} else if ( iMessage == XSSH_MSG_DEBUG ) {
		pPacket->Kind = XSSH_SESSION_PACKET_DEBUG;
		Code = xrtSshDebugRead(Payload, &pPacket->Message.Debug);
	} else if ( iMessage == XSSH_MSG_EXT_INFO ) {
		pPacket->Kind = XSSH_SESSION_PACKET_EXT_INFO;
		Code = xrtSshExtInfoRead(Payload, &pPacket->Message.ExtInfo);
	} else if ( iMessage == XSSH_MSG_NEWCOMPRESS ) {
		pPacket->Kind = XSSH_SESSION_PACKET_NEWCOMPRESS;
		Code = xrtSshNewCompressRead(Payload);
	} else {
		*pRecognized = false;
		pPacket->Kind = XSSH_SESSION_PACKET_EXTENSION;
		Code = XSSH_OK;
	}
	return Code;
}



/* 判断消息号属于标准认证层范围。 */
static bool xsshSessionCoreAuthNumber(uint8 iMessage)
{
	return (iMessage == XSSH_MSG_SERVICE_REQUEST) ||
		(iMessage == XSSH_MSG_SERVICE_ACCEPT) ||
		((iMessage >= XSSH_MSG_USERAUTH_REQUEST) && (iMessage <= 79u));
}



/* 判断消息号属于 RFC 4254 已分配的 connection 范围。 */
static bool xsshSessionCoreConnectionNumber(uint8 iMessage)
{
	return ((iMessage >= XSSH_MSG_GLOBAL_REQUEST) &&
		(iMessage <= XSSH_MSG_REQUEST_FAILURE)) ||
		((iMessage >= XSSH_MSG_CHANNEL_OPEN) &&
		 (iMessage <= XSSH_MSG_CHANNEL_FAILURE));
}



/* 校验 KEX 方法构建器的未决输出与最终 payload 一致。 */
static xsshcode xsshSessionCoreKexWriteCheck(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	uint8 iMessage
)
{
	const xsshkexsession* pKex = xrtSshKexExchangeSessionConst(
		&pSession->Kex
	);
	xsshkexsessionpacket Packet;
	xsshcode Code;

	if ( pKex == NULL || !pKex->Active ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pKex->WritePending;
	if ( ((Packet == XSSH_KEX_PACKET_ECDH_INIT) &&
		 (iMessage != XSSH_MSG_KEX_ECDH_INIT)) ||
		((Packet == XSSH_KEX_PACKET_ECDH_REPLY) &&
		 (iMessage != XSSH_MSG_KEX_ECDH_REPLY)) ||
		((Packet == XSSH_KEX_PACKET_NEWKEYS) &&
		 (iMessage != XSSH_MSG_NEWKEYS)) ||
		((Packet != XSSH_KEX_PACKET_ECDH_INIT) &&
		 (Packet != XSSH_KEX_PACKET_ECDH_REPLY) &&
		 (Packet != XSSH_KEX_PACKET_NEWKEYS)) ) {
		return XSSH_ERROR_STATE;
	}
	Code = iMessage == XSSH_MSG_NEWKEYS ?
		xrtSshTransportNewKeysCheck(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL
		) : xrtSshTransportMessageCheck(
			&pCore->State,
			XSSH_TRANSPORT_LOCAL,
			iMessage
		);
	return Code;
}



/* 完成方向性密钥激活后收口本代动态 KEX transcript。 */
static xsshcode xsshSessionCoreKexFinish(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore
)
{
	if ( !xrtSshKexSessionComplete(&pSession->Kex.Session, pCore) ) {
		return XSSH_OK;
	}
	return xrtSshKexExchangeComplete(&pSession->Kex, pCore);
}



/* 认证完成时自动开放 connection 层，避免两个状态机之间存在空窗。 */
static xsshcode xsshSessionCoreConnectionStart(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ||
		xrtSshConnectionSessionActive(&pSession->Connection) ) {
		return XSSH_OK;
	}
	return xrtSshConnectionSessionBegin(&pSession->Connection, pCore);
}



/* 把 KEX 子状态的动作映射到连接级动作。 */
static xsshsessionaction xsshSessionCoreKexAction(
	const xsshkexsession* pKex
)
{
	switch ( xrtSshKexSessionEvent(pKex) ) {
		case XSSH_KEX_EVENT_WRITE_ECDH_INIT:
			return XSSH_SESSION_ACTION_WRITE_ECDH_INIT;
		case XSSH_KEX_EVENT_READ_ECDH_INIT:
			return XSSH_SESSION_ACTION_READ_ECDH_INIT;
		case XSSH_KEX_EVENT_WRITE_ECDH_REPLY:
			return XSSH_SESSION_ACTION_WRITE_ECDH_REPLY;
		case XSSH_KEX_EVENT_READ_ECDH_REPLY:
			return XSSH_SESSION_ACTION_READ_ECDH_REPLY;
		case XSSH_KEX_EVENT_VERIFY_HOST_KEY:
			return XSSH_SESSION_ACTION_VERIFY_HOST_KEY;
		case XSSH_KEX_EVENT_WRITE_NEWKEYS:
			return XSSH_SESSION_ACTION_WRITE_NEWKEYS;
		case XSSH_KEX_EVENT_READ_NEWKEYS:
			return XSSH_SESSION_ACTION_READ_NEWKEYS;
		case XSSH_KEX_EVENT_ACTIVATE_WRITE:
			return XSSH_SESSION_ACTION_ACTIVATE_WRITE_KEYS;
		case XSSH_KEX_EVENT_ACTIVATE_READ:
			return XSSH_SESSION_ACTION_ACTIVATE_READ_KEYS;
		case XSSH_KEX_EVENT_COMPLETE:
			return XSSH_SESSION_ACTION_COMPLETE_KEX;
		case XSSH_KEX_EVENT_FAILED:
			return XSSH_SESSION_ACTION_FAILED;
		default:
			return XSSH_SESSION_ACTION_NONE;
	}
}



/* 把 USERAUTH 子状态的动作映射到连接级动作。 */
static xsshsessionaction xsshSessionCoreAuthAction(
	const xsshauthsession* pAuth
)
{
	switch ( xrtSshAuthSessionEvent(pAuth) ) {
		case XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST:
			return XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST:
			return XSSH_SESSION_ACTION_READ_SERVICE_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT:
			return XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT;
		case XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT:
			return XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT;
		case XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST:
			return XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_READ_REQUEST:
			return XSSH_SESSION_ACTION_READ_AUTH_REQUEST;
		case XSSH_AUTH_SESSION_EVENT_WRITE_RESULT:
			return XSSH_SESSION_ACTION_WRITE_AUTH_RESULT;
		case XSSH_AUTH_SESSION_EVENT_READ_RESULT:
			return XSSH_SESSION_ACTION_READ_AUTH_RESULT;
		case XSSH_AUTH_SESSION_EVENT_COMPLETE:
			return XSSH_SESSION_ACTION_COMPLETE_AUTH;
		case XSSH_AUTH_SESSION_EVENT_FAILED:
			return XSSH_SESSION_ACTION_FAILED;
		default:
			return XSSH_SESSION_ACTION_NONE;
	}
}



/* 初始化连接级协议所有权。 */
bool xrtSshSessionCoreInit(
	xsshsessioncore* pSession,
	xnetbufpool* pPool,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
)
{
	xsshsessioncore Session;
	size_t iTokenBytes = 0u;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	if ( pGlobalReplies != NULL ) {
		if ( !xrtMemRangeValid(
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || (pGlobalReplies->Capacity >
			(SIZE_MAX / sizeof(uint64))) ) {
			return false;
		}
		iTokenBytes = pGlobalReplies->Capacity * sizeof(uint64);
		if ( xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies,
			sizeof(*pGlobalReplies)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pGlobalReplies->Tokens,
			iTokenBytes
		) ) {
			return false;
		}
	}
	memset(&Session, 0, sizeof(Session));
	if ( !xrtSshKexExchangeInit(&Session.Kex, pPool, Role) ) {
		return false;
	}
	if ( !xrtSshAuthSessionInit(&Session.Auth, Role) ||
		!xrtSshConnectionSessionInit(
			&Session.Connection,
			Role,
			pResolve,
			pUserData,
			pGlobalReplies
		) ) {
		xrtSshKexExchangeClear(&Session.Kex);
		return false;
	}
	Session.Role = Role;
	Session.Initialized = true;
	Session.Guard = XSSH_SESSION_CORE_GUARD;
	*pSession = Session;
	return true;
}



/* 清除全部内部状态和动态 transcript。 */
void xrtSshSessionCoreClear(xsshsessioncore* pSession)
{
	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		return;
	}
	if ( xsshSessionCoreValid(pSession) ) {
		xrtSshKexExchangeClear(&pSession->Kex);
		xrtSshAuthSessionClear(&pSession->Auth);
		xrtSshConnectionSessionClear(&pSession->Connection);
	}
	memset(pSession, 0, sizeof(*pSession));
}



/* 从子状态和 transport 推导连接级阶段。 */
xsshsessionphase xrtSshSessionCorePhase(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		pSession->Failed || xsshSessionCoreChildFailed(pSession) ) {
		return XSSH_SESSION_FAILED;
	}
	if ( (pCore->State.Phase == XSSH_TRANSPORT_CLOSING) ||
		(pCore->State.Phase == XSSH_TRANSPORT_CLOSED) ) {
		return XSSH_SESSION_CLOSING;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_IDENTIFICATION ) {
		return XSSH_SESSION_IDENTIFICATION;
	}
	if ( (pSession->Kex.Phase != XSSH_KEX_EXCHANGE_COMPLETE) ||
		(pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) ) {
		return pSession->Auth.Active ?
			XSSH_SESSION_REKEY : XSSH_SESSION_KEY_EXCHANGE;
	}
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ) {
		return XSSH_SESSION_AUTHENTICATION;
	}
	return XSSH_SESSION_CONNECTION;
}



/* 从所有子层推导唯一的常见驱动动作，不推进任何事务。 */
xsshsessionaction xrtSshSessionCoreAction(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	xsshsessionphase Phase;

	Phase = xrtSshSessionCorePhase(pSession, pCore);
	if ( Phase == XSSH_SESSION_FAILED ) {
		return XSSH_SESSION_ACTION_FAILED;
	}
	if ( Phase == XSSH_SESSION_CLOSING ) {
		return XSSH_SESSION_ACTION_CLOSING;
	}
	if ( (pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		((pSession->Kex.Pending != XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		 (pSession->Kex.PendingDirection == XSSH_TRANSPORT_LOCAL)) ) {
		return XSSH_SESSION_ACTION_WRITE_PENDING;
	}
	if ( (pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		((pSession->Kex.Pending != XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		 (pSession->Kex.PendingDirection == XSSH_TRANSPORT_PEER)) ) {
		return XSSH_SESSION_ACTION_READ_PENDING;
	}
	if ( Phase == XSSH_SESSION_IDENTIFICATION ) {
		return !pCore->State.LocalIdentification ?
			XSSH_SESSION_ACTION_WRITE_IDENTIFICATION :
			XSSH_SESSION_ACTION_READ_IDENTIFICATION;
	}
	if ( (pSession->Kex.Phase == XSSH_KEX_EXCHANGE_KEXINIT) ||
		((pSession->Kex.Phase == XSSH_KEX_EXCHANGE_COMPLETE) &&
		 (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE)) ) {
		return !pCore->State.LocalKexInit ?
			XSSH_SESSION_ACTION_WRITE_KEXINIT :
			XSSH_SESSION_ACTION_READ_KEXINIT;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_READY ) {
		return XSSH_SESSION_ACTION_BEGIN_KEX;
	}
	if ( pSession->Kex.Phase == XSSH_KEX_EXCHANGE_METHOD ) {
		return xsshSessionCoreKexAction(&pSession->Kex.Session);
	}
	if ( !pSession->Auth.Active ) {
		return XSSH_SESSION_ACTION_BEGIN_AUTH;
	}
	if ( !xrtSshAuthSessionComplete(&pSession->Auth, pCore) ) {
		return xsshSessionCoreAuthAction(&pSession->Auth);
	}
	if ( xrtSshConnectionSessionActive(&pSession->Connection) ) {
		return XSSH_SESSION_ACTION_CONNECTION;
	}
	return XSSH_SESSION_ACTION_NONE;
}



/* 返回可变 KEX 子对象。 */
xsshkexexchange* xrtSshSessionCoreKex(xsshsessioncore* pSession)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Kex : NULL;
}



/* 返回只读 KEX 子对象。 */
const xsshkexexchange* xrtSshSessionCoreKexConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Kex : NULL;
}



/* 返回可变认证子对象。 */
xsshauthsession* xrtSshSessionCoreAuth(xsshsessioncore* pSession)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Auth : NULL;
}



/* 返回只读认证子对象。 */
const xsshauthsession* xrtSshSessionCoreAuthConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Auth : NULL;
}



/* 返回可变 connection 子对象。 */
xsshconnectionsession* xrtSshSessionCoreConnection(
	xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Connection : NULL;
}



/* 返回只读 connection 子对象。 */
const xsshconnectionsession* xrtSshSessionCoreConnectionConst(
	const xsshsessioncore* pSession
)
{
	return xsshSessionCoreValid(pSession) && !pSession->Failed ?
		&pSession->Connection : NULL;
}



/* 保存一条尚未提交的 identification。 */
xsshcode xrtSshSessionCoreVersionPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionPrepare(
		&pSession->Kex,
		pCore,
		Direction,
		Version
	);
}



/* 发布 transport 已提交的 identification。 */
xsshcode xrtSshSessionCoreVersionCommit(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionCommit(&pSession->Kex, pCore);
}



/* 放弃尚未提交的 identification。 */
xsshcode xrtSshSessionCoreVersionAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeVersionAbort(&pSession->Kex, pCore);
}



/* 以确定性私钥开始当前 KEX。 */
xsshcode xrtSshSessionCoreKexBeginWithPrivate(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshKexExchangeBeginWithPrivate(
		&pSession->Kex,
		pCore,
		ServerHostKey,
		PrivateKey
	);
}



/* 首轮密钥交换完成后开始认证。 */
xsshcode xrtSshSessionCoreAuthBegin(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->Kex.Phase != XSSH_KEX_EXCHANGE_COMPLETE) ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshAuthSessionBegin(
		&pSession->Auth,
		pCore,
		pPolicy,
		iNowMs
	);
}



/* 在 transport 之前准备本端协议事务。 */
xsshcode xrtSshSessionCoreWritePrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
)
{
	xsshsessionpacket Packet;
	bool bRecognized;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		pCore->Write.Active || pCore->Read.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.LocalPackets == UINT64_MAX) ) {
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
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pKind,
			sizeof(*pKind)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pKind,
			sizeof(*pKind)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pKind,
			sizeof(*pKind)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	memset(&Packet, 0, sizeof(Packet));
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((pChannel != NULL) || (pReplies != NULL)) &&
		!xsshSessionCoreConnectionNumber(iMessage) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iMessage == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitPrepare(
			&pSession->Kex,
			pCore,
			XSSH_TRANSPORT_LOCAL,
			Payload
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEXINIT;
	} else if ( (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		((iMessage == XSSH_MSG_NEWKEYS) ||
		 ((iMessage >= XSSH_KEX_METHOD_MIN) &&
		  (iMessage <= XSSH_KEX_METHOD_MAX))) ) {
		Code = xsshSessionCoreKexWriteCheck(pSession, pCore, iMessage);
		Packet.Kind = XSSH_SESSION_PACKET_KEX;
	} else {
		Code = xsshSessionCoreTransportPacket(
			Payload,
			iMessage,
			&Packet,
			&bRecognized
		);
		if ( (Code == XSSH_OK) && bRecognized ) {
			Code = xrtSshTransportMessageCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL,
				iMessage
			);
		} else if ( Code == XSSH_OK && pSession->Auth.Active &&
			(pSession->Auth.Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
			Code = xrtSshAuthSessionWritePrepare(
				&pSession->Auth,
				pCore,
				Payload,
				iNowMs
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_AUTH;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		} else if ( Code == XSSH_OK &&
			xrtSshConnectionSessionActive(&pSession->Connection) ) {
			Code = xrtSshConnectionSessionWritePrepare(
				&pSession->Connection,
				pCore,
				Payload,
				pChannel,
				pReplies,
				iReplyToken
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_CONNECTION;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		}
		if ( (Code == XSSH_OK) &&
			(Packet.Kind == XSSH_SESSION_PACKET_EXTENSION) ) {
			if ( xsshSessionCoreAuthNumber(iMessage) ||
				xsshSessionCoreConnectionNumber(iMessage) ) {
				return XSSH_ERROR_STATE;
			}
			if ( (pChannel != NULL) || (pReplies != NULL) ) {
				return XSSH_ERROR_ARGUMENT;
			}
			Code = xrtSshTransportMessageCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL,
				iMessage
			);
		}
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	pSession->WritePayload = Payload;
	pSession->WritePending = Packet.Kind;
	pSession->WriteMessage = iMessage;
	*pKind = Packet.Kind;
	return XSSH_OK;
}



/* 把上层候选绑定到 transport 当前未决 packet。 */
xsshcode xrtSshSessionCoreWriteBind(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload
)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		pSession->WriteBound || !pCore->Write.Active || pCore->Read.Active ||
		(pCore->State.LocalPackets >= pSession->WriteOrdinal) ||
		(pCore->Write.Message != pSession->WriteMessage) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Data != pSession->WritePayload.Data) ||
		(Payload.Size != pSession->WritePayload.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT) &&
		 (pCore->Write.Kind != XSSH_TRANSPORT_PACKET_KEXINIT)) ||
		((pSession->WritePending == XSSH_SESSION_PACKET_KEX) &&
		 (pCore->Write.Kind == XSSH_TRANSPORT_PACKET_KEXINIT)) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->WriteBound = true;
	return XSSH_OK;
}



/* 提交 transport 已可靠接受的本端 payload。 */
xsshcode xrtSshSessionCoreWriteCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		!pSession->WriteBound || pCore->Write.Active ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitCommit(&pSession->Kex, pCore);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionWriteCommit(&pSession->Kex.Session, pCore);
		if ( (Code == XSSH_OK) && pSession->Kex.Session.LocalNewKeys &&
			!pSession->Kex.Session.WriteActivated ) {
			Code = xrtSshKexSessionActivateWrite(
				&pSession->Kex.Session,
				pCore,
				iNowMs
			);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreKexFinish(pSession, pCore);
		}
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionWriteCommit(&pSession->Auth, pCore);
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreConnectionStart(pSession, pCore);
		}
	} else if ( pSession->WritePending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionWriteCommit(
			&pSession->Connection,
			pCore
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(pSession);
		return Code;
	}
	xsshSessionCoreWriteClear(pSession);
	return XSSH_OK;
}



/* 放弃尚未可靠提交的本端 payload。 */
xsshcode xrtSshSessionCoreWriteAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->WritePending == XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.LocalPackets >= pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->WritePending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitAbort(&pSession->Kex, pCore);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionWriteAbort(&pSession->Kex.Session);
	} else if ( pSession->WritePending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionWriteAbort(&pSession->Auth);
	} else if ( pSession->WritePending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionWriteAbort(&pSession->Connection);
	}
	if ( Code == XSSH_OK ) {
		xsshSessionCoreWriteClear(pSession);
	}
	return Code;
}



/* 准备 transport 已认证的 peer payload。 */
xsshcode xrtSshSessionCoreReadPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	uint64 iNowMs,
	xsshsessionpacket* pPacket
)
{
	xsshsessionpacket Packet;
	xsshauthsessionpacket AuthPacket;
	bool bRecognized;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		!pCore->Read.Active || pCore->Write.Active ||
		(pSession->WritePending != XSSH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_SESSION_PACKET_NONE) ||
		(pCore->State.PeerPackets == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		!xrtMemRangeValid(pHostKeyStorage, iHostKeyCapacity) ||
		((pHostKeySize != NULL) &&
		 !xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize))) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPacket,
			sizeof(*pPacket)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pHostKeySize != NULL ) {
		*pHostKeySize = 0u;
	}
	memset(&Packet, 0, sizeof(Packet));
	Packet.Payload = Payload;
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( (Code != XSSH_OK) || (iMessage != pCore->Read.Message) ) {
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Packet.Number = iMessage;
	if ( iMessage == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitPrepare(
			&pSession->Kex,
			pCore,
			XSSH_TRANSPORT_PEER,
			Payload
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEXINIT;
	} else if ( (pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		((iMessage == XSSH_MSG_NEWKEYS) ||
		 ((iMessage >= XSSH_KEX_METHOD_MIN) &&
		  (iMessage <= XSSH_KEX_METHOD_MAX))) ) {
		Code = xrtSshKexSessionReadPrepare(
			&pSession->Kex.Session,
			pCore,
			Payload,
			pHostKeyStorage,
			iHostKeyCapacity,
			pHostKeySize
		);
		Packet.Kind = XSSH_SESSION_PACKET_KEX;
		Packet.Message.Kex = pSession->Kex.Session.ReadPending;
	} else {
		Code = xsshSessionCoreTransportPacket(
			Payload,
			iMessage,
			&Packet,
			&bRecognized
		);
		if ( (Code == XSSH_OK) && !bRecognized &&
			pSession->Auth.Active &&
			(pSession->Auth.Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
			Code = xrtSshAuthSessionReadPrepare(
				&pSession->Auth,
				pCore,
				Payload,
				iNowMs,
				&AuthPacket
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_AUTH;
				Packet.Message.Auth = AuthPacket;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		} else if ( (Code == XSSH_OK) && !bRecognized &&
			xrtSshConnectionSessionActive(&pSession->Connection) ) {
			Code = xrtSshConnectionSessionReadPrepare(
				&pSession->Connection,
				pCore,
				Payload,
				&Packet.Message.Connection
			);
			if ( Code == XSSH_OK ) {
				Packet.Kind = XSSH_SESSION_PACKET_CONNECTION;
			} else if ( Code == XSSH_ERROR_UNSUPPORTED ) {
				Code = XSSH_OK;
			}
		}
		if ( (Code == XSSH_OK) &&
			(Packet.Kind == XSSH_SESSION_PACKET_EXTENSION) &&
			(xsshSessionCoreAuthNumber(iMessage) ||
			 xsshSessionCoreConnectionNumber(iMessage)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	pSession->ReadPending = Packet.Kind;
	pSession->ReadMessage = iMessage;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 提交已经由 transport 消费的 peer payload。 */
xsshcode xrtSshSessionCoreReadCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xsshcode Code = XSSH_OK;

	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		!xsshSessionCoreTransportValid(pSession, pCore) ||
		(pSession->ReadPending == XSSH_SESSION_PACKET_NONE) ||
		pCore->Read.Active ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEXINIT ) {
		Code = xrtSshKexExchangeKexInitCommit(&pSession->Kex, pCore);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEX ) {
		Code = xrtSshKexSessionReadCommit(&pSession->Kex.Session, pCore);
		if ( (Code == XSSH_OK) && pSession->Kex.Session.PeerNewKeys &&
			!pSession->Kex.Session.ReadActivated ) {
			Code = xrtSshKexSessionActivateRead(
				&pSession->Kex.Session,
				pCore,
				iNowMs
			);
		}
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreKexFinish(pSession, pCore);
		}
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_AUTH ) {
		Code = xrtSshAuthSessionReadCommit(&pSession->Auth, pCore);
		if ( Code == XSSH_OK ) {
			Code = xsshSessionCoreConnectionStart(pSession, pCore);
		}
	} else if ( pSession->ReadPending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		Code = xrtSshConnectionSessionReadCommit(
			&pSession->Connection,
			pCore
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSshSessionCoreFail(pSession);
		return Code;
	}
	xsshSessionCoreReadClear(pSession);
	return XSSH_OK;
}



/* 放弃不可回滚的 peer payload 并终止全部子状态。 */
xsshcode xrtSshSessionCoreReadAbort(xsshsessioncore* pSession)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ||
		(pSession->ReadPending == XSSH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEXINIT ) {
		xrtNetBufClear(&pSession->Kex.Staging);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_KEX ) {
		(void)xrtSshKexSessionReadAbort(&pSession->Kex.Session);
	} else if ( pSession->ReadPending == XSSH_SESSION_PACKET_AUTH ) {
		(void)xrtSshAuthSessionReadAbort(&pSession->Auth);
	} else if ( pSession->ReadPending ==
		XSSH_SESSION_PACKET_CONNECTION ) {
		(void)xrtSshConnectionSessionReadAbort(&pSession->Connection);
	}
	xrtSshSessionCoreFail(pSession);
	return XSSH_OK;
}



/* 终止协议核心但保留外部资源所有权。 */
void xrtSshSessionCoreFail(xsshsessioncore* pSession)
{
	if ( !xsshSessionCoreValid(pSession) || pSession->Failed ) {
		return;
	}
	xrtSshKexExchangeFail(&pSession->Kex);
	xrtSshAuthSessionFail(&pSession->Auth);
	xrtSshConnectionSessionFail(&pSession->Connection);
	xsshSessionCoreWriteClear(pSession);
	xsshSessionCoreReadClear(pSession);
	pSession->Failed = true;
}

#endif
