#include <string.h>

#include <xrt/ssh_auth_session.h>



#if defined(XSSH_FEATURE_AUTH_SESSION)

#define XSSH_AUTH_SESSION_GUARD UINT32_C(0x41555448)



/* 校验会话公开状态，避免未初始化对象进入事务路径。 */
static bool xsshAuthSessionValid(const xsshauthsession* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->ObjectGuard == XSSH_AUTH_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER)) &&
		(pSession->Phase >= XSSH_AUTH_SESSION_IDLE) &&
		(pSession->Phase <= XSSH_AUTH_SESSION_FAILED) &&
		(pSession->Event >= XSSH_AUTH_SESSION_EVENT_NONE) &&
		(pSession->Event <= XSSH_AUTH_SESSION_EVENT_FAILED) &&
		(pSession->WritePending >= XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->WritePending <= XSSH_AUTH_SESSION_PACKET_METHOD) &&
		(pSession->ReadPending >= XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending <= XSSH_AUTH_SESSION_PACKET_METHOD);
}



/* 比较不以零结尾的协议文本与编译期常量。 */
static bool xsshAuthSessionTextEqual(xstrview Text, const char* sValue)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) &&
		((iSize == 0u) || (memcmp(Text.Data, sValue, iSize) == 0));
}



/* 清除当前待读消息借用视图。 */
static void xsshAuthSessionReadViewsClear(xsshauthsession* pSession)
{
	memset(&pSession->Request, 0, sizeof(pSession->Request));
	memset(&pSession->Failure, 0, sizeof(pSession->Failure));
	memset(&pSession->Banner, 0, sizeof(pSession->Banner));
	pSession->Method = (xbytesview){ NULL, 0u };
}



/* 清除写事务描述，不推进主状态。 */
static void xsshAuthSessionWriteClear(xsshauthsession* pSession)
{
	pSession->WritePending = XSSH_AUTH_SESSION_PACKET_NONE;
	pSession->WriteOrdinal = 0u;
	memset(&pSession->PendingBudget, 0, sizeof(pSession->PendingBudget));
}



/* 清除读事务描述和全部借用视图。 */
static void xsshAuthSessionReadClear(xsshauthsession* pSession)
{
	pSession->ReadPending = XSSH_AUTH_SESSION_PACKET_NONE;
	pSession->ReadOrdinal = 0u;
	xsshAuthSessionReadViewsClear(pSession);
	memset(&pSession->PendingBudget, 0, sizeof(pSession->PendingBudget));
}



/* 失败状态解除所有认证事务，transport 的关闭仍由驱动执行。 */
static void xsshAuthSessionSetFailed(xsshauthsession* pSession)
{
	xsshAuthSessionWriteClear(pSession);
	xsshAuthSessionReadClear(pSession);
	pSession->ContinueAllowed = false;
	pSession->Phase = XSSH_AUTH_SESSION_FAILED;
	pSession->Event = XSSH_AUTH_SESSION_EVENT_FAILED;
}



/* 检查 core 与认证会话的角色、地址和首轮密钥边界。 */
static bool xsshAuthSessionCoreReady(
	const xsshauthsession* pSession,
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
		!pCore->Write.Active && !pCore->Read.Active &&
		xrtSshTransportCoreKexComplete(pCore);
}



/* 返回当前角色应观察的 server USERAUTH_SUCCESS 方向。 */
static bool xsshAuthSessionCoreSuccess(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	return pSession->Role == XSSH_ROLE_SERVER ?
		pCore->State.LocalAuthSuccess : pCore->State.PeerAuthSuccess;
}



/* 将完整 payload 严格分类，并解析通用认证视图。 */
static xsshcode xsshAuthSessionPacketRead(
	xbytesview Payload,
	xsshauthsessionpacket* pPacket,
	xsshauthrequest* pRequest,
	xsshauthfailure* pFailure,
	xsshauthbanner* pBanner
)
{
	xsshservice Service;
	uint8 iMessage;
	xsshcode Code;

	memset(pRequest, 0, sizeof(*pRequest));
	memset(pFailure, 0, sizeof(*pFailure));
	memset(pBanner, 0, sizeof(*pBanner));
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_SERVICE_REQUEST ) {
		Code = xrtSshServiceRequestRead(Payload, &Service);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
	} else if ( iMessage == XSSH_MSG_SERVICE_ACCEPT ) {
		Code = xrtSshServiceAcceptRead(Payload, &Service);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	} else if ( iMessage == XSSH_MSG_USERAUTH_REQUEST ) {
		Code = xrtSshAuthRequestRead(Payload, pRequest);
		*pPacket = XSSH_AUTH_SESSION_PACKET_REQUEST;
	} else if ( iMessage == XSSH_MSG_USERAUTH_FAILURE ) {
		Code = xrtSshAuthFailureRead(Payload, pFailure);
		*pPacket = XSSH_AUTH_SESSION_PACKET_FAILURE;
	} else if ( iMessage == XSSH_MSG_USERAUTH_SUCCESS ) {
		Code = xrtSshAuthSuccessRead(Payload);
		*pPacket = XSSH_AUTH_SESSION_PACKET_SUCCESS;
	} else if ( iMessage == XSSH_MSG_USERAUTH_BANNER ) {
		Code = xrtSshAuthBannerRead(Payload, pBanner);
		*pPacket = XSSH_AUTH_SESSION_PACKET_BANNER;
	} else if ( (iMessage >= 60u) && (iMessage <= 79u) ) {
		Code = XSSH_OK;
		*pPacket = XSSH_AUTH_SESSION_PACKET_METHOD;
	} else {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((*pPacket == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST) ||
		 (*pPacket == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT)) &&
		!xsshAuthSessionTextEqual(Service.Name, XSSH_SERVICE_USERAUTH) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( (*pPacket == XSSH_AUTH_SESSION_PACKET_REQUEST) &&
		!xsshAuthSessionTextEqual(
			pRequest->Service,
			XSSH_SERVICE_CONNECTION
		) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return XSSH_OK;
}



/* 判断当前驱动事件是否允许发送指定分类。 */
static bool xsshAuthSessionWriteAllowed(
	const xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_BANNER) &&
		(pSession->Role == XSSH_ROLE_SERVER) &&
		pSession->ServiceAccepted &&
		(pSession->Phase == XSSH_AUTH_SESSION_AUTHENTICATION) ) {
		return true;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( pSession->Event ==
			XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST ) {
			return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
		}
		if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST ) {
			return (Packet == XSSH_AUTH_SESSION_PACKET_REQUEST) ||
				(pSession->ContinueAllowed &&
				 (Packet == XSSH_AUTH_SESSION_PACKET_METHOD));
		}
		return false;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT ) {
		return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_WRITE_RESULT ) {
		return (Packet == XSSH_AUTH_SESSION_PACKET_FAILURE) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_METHOD);
	}
	return false;
}



/* 判断当前驱动事件是否允许接收指定分类。 */
static bool xsshAuthSessionReadAllowed(
	const xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_BANNER) &&
		(pSession->Role == XSSH_ROLE_CLIENT) &&
		pSession->ServiceAccepted &&
		(pSession->Phase == XSSH_AUTH_SESSION_AUTHENTICATION) ) {
		return true;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( pSession->Event ==
			XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST ) {
			return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST;
		}
		if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_REQUEST ) {
			return (Packet == XSSH_AUTH_SESSION_PACKET_REQUEST) ||
				(pSession->ContinueAllowed &&
				 (Packet == XSSH_AUTH_SESSION_PACKET_METHOD));
		}
		return false;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT ) {
		return Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT;
	}
	if ( pSession->Event == XSSH_AUTH_SESSION_EVENT_READ_RESULT ) {
		return (Packet == XSSH_AUTH_SESSION_PACKET_FAILURE) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) ||
			(Packet == XSSH_AUTH_SESSION_PACKET_METHOD);
	}
	return false;
}



/* 把消息分类转换成认证预算事件。 */
static xsshauthevent xsshAuthSessionBudgetEvent(
	xsshauthsessionpacket Packet,
	bool bServerMessage
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_REQUEST ) {
		return XSSH_AUTH_EVENT_ATTEMPT;
	}
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_METHOD) && bServerMessage ) {
		return XSSH_AUTH_EVENT_ROUND;
	}
	return XSSH_AUTH_EVENT_MESSAGE;
}



/* 在临时副本中预留预算，事务提交前不修改正式计数。 */
static xsshcode xsshAuthSessionBudgetPrepare(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet,
	bool bServerMessage,
	size_t iPayloadSize,
	uint64 iNowMs
)
{
	xsshauthguarddecision Decision;
	xsshauthguard Budget = pSession->Budget;
	xsshcode Code;

	if ( (Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST) ||
		(Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT) ) {
		pSession->PendingBudget = Budget;
		return XSSH_OK;
	}
	Code = xrtSshAuthGuardReserve(
		&Budget,
		xsshAuthSessionBudgetEvent(Packet, bServerMessage),
		(uint64)iPayloadSize,
		iNowMs,
		&Decision
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision != XSSH_AUTH_GUARD_ALLOW ) {
		pSession->Budget = Budget;
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_AUTHENTICATION;
	}
	pSession->PendingBudget = Budget;
	return XSSH_OK;
}



/* 提交成功分类时同时冻结认证预算。 */
static bool xsshAuthSessionBudgetCommit(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( (Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		!xrtSshAuthGuardComplete(&pSession->PendingBudget) ) {
		return false;
	}
	pSession->Budget = pSession->PendingBudget;
	return true;
}



/* 按可靠写边界推进 client/server 认证步骤。 */
static void xsshAuthSessionWriteAdvance(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_BANNER ) {
		return;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST ) {
			pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT;
		} else {
			pSession->ContinueAllowed = false;
			pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_RESULT;
		}
		return;
	}
	if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT ) {
		pSession->ServiceAccepted = true;
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_AUTHENTICATION;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_REQUEST;
	} else if ( Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS ) {
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_COMPLETE;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_COMPLETE;
	} else {
		pSession->ContinueAllowed =
			Packet == XSSH_AUTH_SESSION_PACKET_METHOD;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_READ_REQUEST;
	}
}



/* 按认证输入提交边界推进 client/server 认证步骤。 */
static void xsshAuthSessionReadAdvance(
	xsshauthsession* pSession,
	xsshauthsessionpacket Packet
)
{
	if ( Packet == XSSH_AUTH_SESSION_PACKET_BANNER ) {
		return;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST ) {
			pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT;
		} else {
			pSession->ContinueAllowed = false;
			pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_RESULT;
		}
		return;
	}
	if ( Packet == XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT ) {
		pSession->ServiceAccepted = true;
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_AUTHENTICATION;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST;
	} else if ( Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS ) {
		pSession->ContinueAllowed = false;
		pSession->Phase = XSSH_AUTH_SESSION_COMPLETE;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_COMPLETE;
	} else {
		pSession->ContinueAllowed =
			Packet == XSSH_AUTH_SESSION_PACKET_METHOD;
		pSession->Event = XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST;
	}
}



/* 判断输出对象是否会覆盖会话或当前借用输入。 */
static bool xsshAuthSessionOutputOverlap(
	const xsshauthsession* pSession,
	const void* pOutput,
	size_t iOutputSize
)
{
	return xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.User.Data,
		pSession->Request.User.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Service.Data,
		pSession->Request.Service.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Method.Data,
		pSession->Request.Method.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Request.Fields.Data,
		pSession->Request.Fields.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Failure.Methods.Data,
		pSession->Failure.Methods.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Banner.Message.Data,
		pSession->Banner.Message.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Banner.Language.Data,
		pSession->Banner.Language.Size,
		pOutput,
		iOutputSize
	) || xrtMemRangesOverlap(
		pSession->Method.Data,
		pSession->Method.Size,
		pOutput,
		iOutputSize
	);
}



/* 初始化空认证会话。 */
bool xrtSshAuthSessionInit(xsshauthsession* pSession, xsshrole Role)
{
	xsshauthsession Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Phase = XSSH_AUTH_SESSION_IDLE;
	Session.ObjectGuard = XSSH_AUTH_SESSION_GUARD;
	*pSession = Session;
	return true;
}



/* 清除认证状态和借用视图。 */
void xrtSshAuthSessionClear(xsshauthsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		memset(pSession, 0, sizeof(*pSession));
	}
}



/* 从已经完成首轮 KEX 的 transport 开始认证。 */
xsshcode xrtSshAuthSessionBegin(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
)
{
	xsshauthsession Session;

	if ( !xsshAuthSessionValid(pSession) ||
		!xsshAuthSessionCoreReady(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pPolicy != NULL) &&
		(!xrtMemRangeValid(pPolicy, sizeof(*pPolicy)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pPolicy,
			sizeof(*pPolicy)
		 ) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPolicy,
			sizeof(*pPolicy)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active ||
		(pSession->Phase != XSSH_AUTH_SESSION_IDLE) ||
		xsshAuthSessionCoreSuccess(pSession, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Session = *pSession;
	if ( !xrtSshAuthGuardInit(&Session.Budget, pPolicy, iNowMs) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Session.Active = true;
	Session.Phase = XSSH_AUTH_SESSION_SERVICE;
	Session.Event = Session.Role == XSSH_ROLE_CLIENT ?
		XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST :
		XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST;
	*pSession = Session;
	return XSSH_OK;
}



/* 查询下一认证动作。 */
xsshauthsessionevent xrtSshAuthSessionEvent(
	const xsshauthsession* pSession
)
{
	return xsshAuthSessionValid(pSession) ?
		pSession->Event : XSSH_AUTH_SESSION_EVENT_NONE;
}



/* 复制资源预算快照。 */
xsshcode xrtSshAuthSessionBudget(
	const xsshauthsession* pSession,
	xsshauthguard* pBudget
)
{
	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBudget, sizeof(*pBudget)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pBudget,
			sizeof(*pBudget)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBudget = pSession->Budget;
	return XSSH_OK;
}



/* 检查无外部时钟依赖的认证预算。 */
xsshcode xrtSshAuthSessionCheck(
	xsshauthsession* pSession,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
)
{
	xsshauthguarddecision Decision;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthGuardCheck(&pSession->Budget, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pDecision = Decision;
	if ( Decision == XSSH_AUTH_GUARD_DISCONNECT ) {
		xsshAuthSessionSetFailed(pSession);
	}
	return XSSH_OK;
}



/* 准备不复制 payload 的认证写事务。 */
xsshcode xrtSshAuthSessionWritePrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint64 iNowMs
)
{
	xsshauthsessionpacket Packet = XSSH_AUTH_SESSION_PACKET_NONE;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ||
		(pSession->WritePending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xsshAuthSessionCoreReady(pSession, pCore) ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_LOCAL
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
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
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshAuthSessionPacketRead(
		Payload,
		&Packet,
		&Request,
		&Failure,
		&Banner
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthSessionWriteAllowed(pSession, Packet) ||
		(pCore->State.LocalPackets == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshAuthSessionBudgetPrepare(
		pSession,
		Packet,
		pSession->Role == XSSH_ROLE_SERVER,
		Payload.Size,
		iNowMs
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->WritePending = Packet;
	pSession->WriteOrdinal = pCore->State.LocalPackets + 1u;
	return XSSH_OK;
}



/* 提交 transport 已可靠发送的认证消息。 */
xsshcode xrtSshAuthSessionWriteCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshauthsessionpacket Packet;

	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		pCore->Write.Active ||
		(pCore->State.LocalPackets != pSession->WriteOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->WritePending;
	if ( ((Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		 !xsshAuthSessionCoreSuccess(pSession, pCore)) ||
		!xsshAuthSessionBudgetCommit(pSession, Packet) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionWriteAdvance(pSession, Packet);
	xsshAuthSessionWriteClear(pSession);
	return XSSH_OK;
}



/* 无损放弃未提交的认证输出。 */
xsshcode xrtSshAuthSessionWriteAbort(xsshauthsession* pSession)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionWriteClear(pSession);
	return XSSH_OK;
}



/* 准备一个已经由 transport core 认证的输入事务。 */
xsshcode xrtSshAuthSessionReadPrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint64 iNowMs,
	xsshauthsessionpacket* pPacket
)
{
	xsshauthsessionpacket Packet = XSSH_AUTH_SESSION_PACKET_NONE;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshAuthSessionValid(pSession) || !pSession->Active ||
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) ||
		(pSession->Phase == XSSH_AUTH_SESSION_FAILED) ||
		(pSession->WritePending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		!pCore->Read.Active ||
		!xrtSshTransportCoreCanApplication(
			pCore,
			XSSH_TRANSPORT_PEER
		) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
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
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( (Code != XSSH_OK) || (iMessage != pCore->Read.Message) ) {
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshAuthSessionPacketRead(
		Payload,
		&Packet,
		&Request,
		&Failure,
		&Banner
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshAuthSessionReadAllowed(pSession, Packet) ||
		(pCore->State.PeerPackets == UINT64_MAX) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xsshAuthSessionBudgetPrepare(
		pSession,
		Packet,
		pSession->Role == XSSH_ROLE_CLIENT,
		Payload.Size,
		iNowMs
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pSession->Request = Request;
	pSession->Failure = Failure;
	pSession->Banner = Banner;
	if ( Packet == XSSH_AUTH_SESSION_PACKET_METHOD ) {
		pSession->Method = Payload;
	}
	pSession->ReadPending = Packet;
	pSession->ReadOrdinal = pCore->State.PeerPackets + 1u;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 返回当前通用认证请求。 */
xsshcode xrtSshAuthSessionRequest(
	const xsshauthsession* pSession,
	xsshauthrequest* pRequest
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_REQUEST) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pRequest, sizeof(*pRequest)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pRequest,
			sizeof(*pRequest)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pRequest = pSession->Request;
	return XSSH_OK;
}



/* 返回当前认证失败方法列表。 */
xsshcode xrtSshAuthSessionFailure(
	const xsshauthsession* pSession,
	xsshauthfailure* pFailure
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_FAILURE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pFailure, sizeof(*pFailure)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pFailure,
			sizeof(*pFailure)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pFailure = pSession->Failure;
	return XSSH_OK;
}



/* 返回当前认证横幅。 */
xsshcode xrtSshAuthSessionBanner(
	const xsshauthsession* pSession,
	xsshauthbanner* pBanner
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_BANNER) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pBanner, sizeof(*pBanner)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pBanner,
			sizeof(*pBanner)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pBanner = pSession->Banner;
	return XSSH_OK;
}



/* 返回当前方法专用 payload。 */
xsshcode xrtSshAuthSessionMethod(
	const xsshauthsession* pSession,
	xbytesview* pPayload
)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending != XSSH_AUTH_SESSION_PACKET_METHOD) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pPayload, sizeof(*pPayload)) ||
		xsshAuthSessionOutputOverlap(
			pSession,
			pPayload,
			sizeof(*pPayload)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pPayload = pSession->Method;
	return XSSH_OK;
}



/* 提交 transport 已接受的认证输入。 */
xsshcode xrtSshAuthSessionReadCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshauthsessionpacket Packet;

	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || (pCore->State.Role != pSession->Role) ||
		pCore->Read.Active ||
		(pCore->State.PeerPackets != pSession->ReadOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->ReadPending;
	if ( ((Packet == XSSH_AUTH_SESSION_PACKET_SUCCESS) &&
		 !xsshAuthSessionCoreSuccess(pSession, pCore)) ||
		!xsshAuthSessionBudgetCommit(pSession, Packet) ) {
		xsshAuthSessionSetFailed(pSession);
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionReadAdvance(pSession, Packet);
	xsshAuthSessionReadClear(pSession);
	return XSSH_OK;
}



/* 已认证输入不可回滚，放弃时终止会话。 */
xsshcode xrtSshAuthSessionReadAbort(xsshauthsession* pSession)
{
	if ( !xsshAuthSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshAuthSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 显式终止认证状态机。 */
void xrtSshAuthSessionFail(xsshauthsession* pSession)
{
	if ( xsshAuthSessionValid(pSession) && pSession->Active &&
		(pSession->Phase != XSSH_AUTH_SESSION_COMPLETE) ) {
		xsshAuthSessionSetFailed(pSession);
	}
}



/* 校验认证会话与 transport 的 server 成功方向一致。 */
bool xrtSshAuthSessionComplete(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xsshAuthSessionValid(pSession) && pSession->Active &&
		xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pSession->Role) &&
		(pSession->Phase == XSSH_AUTH_SESSION_COMPLETE) &&
		(pSession->Event == XSSH_AUTH_SESSION_EVENT_COMPLETE) &&
		(pSession->WritePending == XSSH_AUTH_SESSION_PACKET_NONE) &&
		(pSession->ReadPending == XSSH_AUTH_SESSION_PACKET_NONE) &&
		pSession->Budget.Complete &&
		xsshAuthSessionCoreSuccess(pSession, pCore) &&
		xrtSshTransportCoreKexComplete(pCore);
}

#endif
