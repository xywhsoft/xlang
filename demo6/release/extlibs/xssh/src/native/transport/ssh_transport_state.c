#include <string.h>

#include <xrt/ssh_transport_state.h>



#if defined(XSSH_FEATURE_TRANSPORT_STATE)

#define XSSH_TRANSPORT_RULES_GUARD UINT32_C(0x5853524c)
#define XSSH_TRANSPORT_STATE_GUARD UINT32_C(0x58535354)



/* 判断 endpoint 角色属于公开枚举。 */
static bool xsshTransportRoleValid(xsshrole Role)
{
	return (Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER);
}



/* 判断消息方向属于公开枚举。 */
static bool xsshTransportDirectionValid(xsshtransportdirection Direction)
{
	return (Direction == XSSH_TRANSPORT_LOCAL) ||
		(Direction == XSSH_TRANSPORT_PEER);
}



/* 判断 KEX 方法规则已初始化。 */
static bool xsshTransportRulesValid(const xsshtransportkexrules* pRules)
{
	return (pRules != NULL) &&
		(pRules->Guard == XSSH_TRANSPORT_RULES_GUARD);
}



/* 判断 transport 状态可继续使用。 */
static bool xsshTransportStateValid(const xsshtransportstate* pState)
{
	return (pState != NULL) &&
		(pState->Guard == XSSH_TRANSPORT_STATE_GUARD) &&
		xsshTransportRoleValid(pState->Role) &&
		(pState->Phase >= XSSH_TRANSPORT_IDENTIFICATION) &&
		(pState->Phase <= XSSH_TRANSPORT_CLOSED);
}



/* 本端非法调用返回状态错误，对端非法线路状态返回协议错误。 */
static xsshcode xsshTransportDirectionError(
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		XSSH_ERROR_STATE : XSSH_ERROR_PROTOCOL;
}



/* 返回指定方向已经提交的 packet 数。 */
static uint64 xsshTransportPackets(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalPackets : pState->PeerPackets;
}



/* 增加指定方向 packet 数；调用方已经完成溢出检查。 */
static void xsshTransportPacketIncrement(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalPackets++;
	} else {
		pState->PeerPackets++;
	}
}



/* 检查计数器以及 strict-kex 初始阶段不得发生 uint32 序列回绕。 */
static xsshcode xsshTransportPacketCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	uint64 iPackets = xsshTransportPackets(pState, Direction);

	if ( iPackets == UINT64_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( pState->Strict && (pState->KexCount == 0u) &&
		(iPackets >= (uint64)UINT32_MAX) ) {
		return xsshTransportDirectionError(Direction);
	}
	return XSSH_OK;
}



/* 判断对应方向已经提交 KEXINIT 但尚未提交 NEWKEYS。 */
static bool xsshTransportDirectionKexActive(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->LocalKexInit && !pState->LocalNewKeys) :
		(pState->PeerKexInit && !pState->PeerNewKeys);
}



/* 返回指定方向是否已经提交 KEXINIT。 */
static bool xsshTransportDirectionKexInit(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexInit : pState->PeerKexInit;
}



/* 返回指定方向是否已经提交 NEWKEYS。 */
static bool xsshTransportDirectionNewKeys(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalNewKeys : pState->PeerNewKeys;
}



/* 判断第二次 EXT_INFO 后是否正在等待紧邻的 USERAUTH_SUCCESS。 */
static bool xsshTransportAuthSuccessPending(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalAuthSuccessPending :
		pState->PeerAuthSuccessPending;
}



/* 判断指定 server 方向已经提交 USERAUTH_SUCCESS。 */
static bool xsshTransportAuthSuccess(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalAuthSuccess : pState->PeerAuthSuccess;
}



/* 返回指定方向的 KEX 方法剩余额度。 */
static const uint8* xsshTransportKexRemainingConst(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexRemaining : pState->PeerKexRemaining;
}



/* 返回指定方向可修改的 KEX 方法剩余额度。 */
static uint8* xsshTransportKexRemaining(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalKexRemaining : pState->PeerKexRemaining;
}



/* 判断指定方向的全部 KEX 方法额度均已消费。 */
static bool xsshTransportKexComplete(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	const uint8* pRemaining = xsshTransportKexRemainingConst(
		pState,
		Direction
	);
	size_t i;

	for ( i = 0u; i < XSSH_KEX_METHOD_COUNT; ++i ) {
		if ( pRemaining[i] != 0u ) {
			return false;
		}
	}
	return true;
}



/* 判断指定方向是否仍欠一包 first_kex_packet_follows 猜测消息。 */
static bool xsshTransportGuessPending(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->LocalGuessExpected && !pState->LocalGuessSeen) :
		(pState->PeerGuessExpected && !pState->PeerGuessSeen);
}



/* 消费一项已经验证存在的 KEX 方法额度。 */
static void xsshTransportKexConsume(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	xsshTransportKexRemaining(pState, Direction)[
		(size_t)iMessage - XSSH_KEX_METHOD_MIN
	]--;
}



/* 关闭指定方向 NEWKEYS 后的第一次 EXT_INFO 机会。 */
static void xsshTransportFirstExtClose(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalFirstExtOpen = false;
	} else {
		pState->PeerFirstExtOpen = false;
	}
}



/* 判断指定方向是否具备发送或接收应用消息的密钥边界。 */
static bool xsshTransportCanApplicationInternal(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN) ) {
		return false;
	}
	if ( pState->Phase == XSSH_TRANSPORT_OPEN ) {
		return true;
	}
	if ( pState->KexCount == 0u ) {
		return xsshTransportDirectionNewKeys(pState, Direction);
	}
	return !xsshTransportDirectionKexInit(pState, Direction) ||
		xsshTransportDirectionNewKeys(pState, Direction);
}



/* 开始下一代 rekey，并保留连接级 strict 与 EXT_INFO 能力。 */
static void xsshTransportBeginRekey(xsshtransportstate* pState)
{
	memset(
		pState->LocalKexRemaining,
		0,
		sizeof(pState->LocalKexRemaining)
	);
	memset(
		pState->PeerKexRemaining,
		0,
		sizeof(pState->PeerKexRemaining)
	);
	pState->LocalKexInit = false;
	pState->PeerKexInit = false;
	pState->LocalNewKeys = false;
	pState->PeerNewKeys = false;
	pState->KexConfigured = false;
	pState->LocalGuessMessage = 0u;
	pState->PeerGuessMessage = 0u;
	pState->LocalGuessExpected = false;
	pState->PeerGuessExpected = false;
	pState->LocalGuessSeen = false;
	pState->PeerGuessSeen = false;
	pState->LocalGuessSkip = false;
	pState->PeerGuessSkip = false;
	pState->LocalStrictViolation = false;
	pState->PeerStrictViolation = false;
	pState->Phase = XSSH_TRANSPORT_KEY_EXCHANGE;
}



/* 检查 EXT_INFO 是否处于首次或 server 认证成功前的第二次机会。 */
static xsshcode xsshTransportExtInfoCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bEnabled = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->SendExtInfo : pState->AcceptExtInfo;
	bool bFirstOpen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalFirstExtOpen : pState->PeerFirstExtOpen;
	bool bSecondUsed = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalSecondExtUsed : pState->PeerSecondExtUsed;
	bool bServerDirection = Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->Role == XSSH_ROLE_SERVER) :
		(pState->Role == XSSH_ROLE_CLIENT);

	if ( !bEnabled ||
		!xsshTransportCanApplicationInternal(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( bFirstOpen ) {
		return XSSH_OK;
	}
	if ( !bServerDirection || bSecondUsed ||
		(pState->KexCount == 0u) ||
		xsshTransportAuthSuccessPending(pState, Direction) ||
		xsshTransportAuthSuccess(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	return XSSH_OK;
}



/* 提交 EXT_INFO 的首次或第二次机会状态。 */
static void xsshTransportExtInfoCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bFirstOpen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalFirstExtOpen : pState->PeerFirstExtOpen;

	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( bFirstOpen ) {
			pState->LocalFirstExtOpen = false;
			pState->LocalFirstExtUsed = true;
		} else {
			pState->LocalSecondExtUsed = true;
			pState->LocalAuthSuccessPending = true;
		}
	} else if ( bFirstOpen ) {
		pState->PeerFirstExtOpen = false;
		pState->PeerFirstExtUsed = true;
	} else {
		pState->PeerSecondExtUsed = true;
		pState->PeerAuthSuccessPending = true;
	}
}



/* 初始化空规则并发布 guard。 */
bool xrtSshTransportKexRulesInit(xsshtransportkexrules* pRules)
{
	xsshtransportkexrules Rules;

	if ( !xrtMemRangeValid(pRules, sizeof(*pRules)) ) {
		return false;
	}
	memset(&Rules, 0, sizeof(Rules));
	Rules.Guard = XSSH_TRANSPORT_RULES_GUARD;
	*pRules = Rules;
	return true;
}



/* 设置方法消息的精确方向额度。 */
bool xrtSshTransportKexRuleSet(
	xsshtransportkexrules* pRules,
	xsshtransportdirection Direction,
	uint8 iMessage,
	uint8 iCount
)
{
	size_t iIndex;

	if ( !xsshTransportRulesValid(pRules) ||
		!xsshTransportDirectionValid(Direction) ||
		(iMessage < XSSH_KEX_METHOD_MIN) ||
		(iMessage > XSSH_KEX_METHOD_MAX) ) {
		return false;
	}
	iIndex = (size_t)iMessage - XSSH_KEX_METHOD_MIN;
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pRules->Local[iIndex] = iCount;
	} else {
		pRules->Peer[iIndex] = iCount;
	}
	return true;
}



/* 初始化 identification 阶段。 */
bool xrtSshTransportStateInit(
	xsshtransportstate* pState,
	xsshrole Role
)
{
	xsshtransportstate State;

	if ( !xrtMemRangeValid(pState, sizeof(*pState)) ||
		!xsshTransportRoleValid(Role) ) {
		return false;
	}
	memset(&State, 0, sizeof(State));
	State.Role = Role;
	State.Phase = XSSH_TRANSPORT_IDENTIFICATION;
	State.Guard = XSSH_TRANSPORT_STATE_GUARD;
	*pState = State;
	return true;
}



/* 清除状态对象本身。 */
void xrtSshTransportStateClear(xsshtransportstate* pState)
{
	if ( pState != NULL ) {
		memset(pState, 0, sizeof(*pState));
	}
}



/* 提交一个 identification 方向，双方完成后进入首次 KEX。 */
xsshcode xrtSshTransportIdentificationCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool* pCommitted;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pState->Phase != XSSH_TRANSPORT_IDENTIFICATION ) {
		return xsshTransportDirectionError(Direction);
	}
	pCommitted = Direction == XSSH_TRANSPORT_LOCAL ?
		&pState->LocalIdentification : &pState->PeerIdentification;
	if ( *pCommitted ) {
		return xsshTransportDirectionError(Direction);
	}
	*pCommitted = true;
	if ( pState->LocalIdentification && pState->PeerIdentification ) {
		pState->Phase = XSSH_TRANSPORT_KEY_EXCHANGE;
	}
	return XSSH_OK;
}



/* 查询方向性应用消息能力。 */
bool xrtSshTransportCanApplication(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	return xsshTransportStateValid(pState) &&
		xsshTransportDirectionValid(Direction) &&
		xsshTransportCanApplicationInternal(pState, Direction);
}



/* 对端先发起 rekey 时提示本端回复。 */
bool xrtSshTransportKexReplyNeeded(const xsshtransportstate* pState)
{
	return xsshTransportStateValid(pState) &&
		(pState->Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		pState->PeerKexInit && !pState->LocalKexInit;
}



/* 检查指定方向 KEXINIT 边界。 */
xsshcode xrtSshTransportKexInitCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN)) ||
		xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (pState->Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(xsshTransportDirectionKexInit(pState, Direction) ||
		xsshTransportDirectionNewKeys(pState, Direction)) ) {
		return xsshTransportDirectionError(Direction);
	}
	return xsshTransportPacketCheck(pState, Direction);
}



/* 提交 KEXINIT，并建立 guessed-packet 期望。 */
xsshcode xrtSshTransportKexInitCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bFirstKexPacketFollows
)
{
	xsshcode Code = xrtSshTransportKexInitCheck(pState, Direction);
	uint64 iOrdinal;

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pState->Phase == XSSH_TRANSPORT_OPEN ) {
		xsshTransportBeginRekey(pState);
	}
	xsshTransportFirstExtClose(pState, Direction);
	iOrdinal = xsshTransportPackets(pState, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalKexInit = true;
		pState->LocalGuessExpected = bFirstKexPacketFollows;
		pState->LocalKexInitOrdinal = iOrdinal;
	} else {
		pState->PeerKexInit = true;
		pState->PeerGuessExpected = bFirstKexPacketFollows;
		pState->PeerKexInitOrdinal = iOrdinal;
	}
	xsshTransportPacketIncrement(pState, Direction);
	return XSSH_OK;
}



/* 验证并消费配置前已经提交的猜测包。 */
static xsshcode xsshTransportConfigureGuess(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bSkip
)
{
	bool bSeen = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessSeen : pState->PeerGuessSeen;
	uint8 iMessage = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessMessage : pState->PeerGuessMessage;
	uint8* pRemaining;

	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalGuessSkip = bSkip;
	} else {
		pState->PeerGuessSkip = bSkip;
	}
	if ( !bSeen || bSkip ) {
		return XSSH_OK;
	}
	pRemaining = xsshTransportKexRemaining(pState, Direction);
	if ( pRemaining[(size_t)iMessage - XSSH_KEX_METHOD_MIN] == 0u ) {
		return xsshTransportDirectionError(Direction);
	}
	xsshTransportKexConsume(pState, Direction, iMessage);
	return XSSH_OK;
}



/* 比较调用方协商结果与按 client 优先级重新计算的结果。 */
static bool xsshTransportTextEqual(xstrview Left, xstrview Right)
{
	if ( ((Left.Data == NULL) && (Left.Size != 0u)) ||
		((Right.Data == NULL) && (Right.Size != 0u)) ) {
		return false;
	}
	return (Left.Size == Right.Size) &&
		((Left.Size == 0u) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较全部双向 KEX 协商字段。 */
static bool xsshTransportNegotiationEqual(
	const xsshkexnegotiation* pLeft,
	const xsshkexnegotiation* pRight
)
{
	return xsshTransportTextEqual(
		pLeft->KexAlgorithm,
		pRight->KexAlgorithm
	) && xsshTransportTextEqual(
		pLeft->ServerHostKeyAlgorithm,
		pRight->ServerHostKeyAlgorithm
	) && xsshTransportTextEqual(
		pLeft->CipherClientToServer,
		pRight->CipherClientToServer
	) && xsshTransportTextEqual(
		pLeft->CipherServerToClient,
		pRight->CipherServerToClient
	) && xsshTransportTextEqual(
		pLeft->MacClientToServer,
		pRight->MacClientToServer
	) && xsshTransportTextEqual(
		pLeft->MacServerToClient,
		pRight->MacServerToClient
	) && xsshTransportTextEqual(
		pLeft->CompressionClientToServer,
		pRight->CompressionClientToServer
	) && xsshTransportTextEqual(
		pLeft->CompressionServerToClient,
		pRight->CompressionServerToClient
	);
}



/* 提交协商、严格模式和本代方法额度。 */
xsshcode xrtSshTransportKexConfigure(
	xsshtransportstate* pState,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
)
{
	xsshtransportstate State;
	xsshkexfeatures Features;
	xsshkexnegotiation Expected;
	bool bLocalSkip;
	bool bPeerSkip;
	bool bInitial;
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pPeer, sizeof(*pPeer)) ||
		!xrtMemRangeValid(pNegotiation, sizeof(*pNegotiation)) ||
		!xsshTransportRulesValid(pRules) ||
		xrtMemRangesOverlap(pState, sizeof(*pState), pLocal, sizeof(*pLocal)) ||
		xrtMemRangesOverlap(pState, sizeof(*pState), pPeer, sizeof(*pPeer)) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pNegotiation,
			sizeof(*pNegotiation)
		) || xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pRules,
			sizeof(*pRules)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!pState->LocalKexInit || !pState->PeerKexInit ||
		pState->KexConfigured ) {
		return XSSH_ERROR_STATE;
	}
	if ( pLocal->FirstKexPacketFollows != pState->LocalGuessExpected ) {
		return XSSH_ERROR_STATE;
	}
	if ( pPeer->FirstKexPacketFollows != pState->PeerGuessExpected ) {
		return XSSH_ERROR_PROTOCOL;
	}
	bInitial = pState->KexCount == 0u;
	memset(&Features, 0, sizeof(Features));
	if ( bInitial ) {
		Code = xrtSshKexFeatures(
			pLocal,
			pPeer,
			pState->Role,
			true,
			&Features
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	memset(&Expected, 0, sizeof(Expected));
	Code = pState->Role == XSSH_ROLE_CLIENT ?
		xrtSshKexNegotiate(pLocal, pPeer, &Expected) :
		xrtSshKexNegotiate(pPeer, pLocal, &Expected);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshTransportNegotiationEqual(&Expected, pNegotiation) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshKexGuessSkip(pLocal, pNegotiation, &bLocalSkip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshKexGuessSkip(pPeer, pNegotiation, &bPeerSkip);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Features.Strict &&
		((pState->LocalKexInitOrdinal != 0u) ||
		 (pState->PeerKexInitOrdinal != 0u) ||
		 pState->LocalStrictViolation || pState->PeerStrictViolation ||
		 (pState->LocalPackets > (uint64)UINT32_MAX) ||
		 (pState->PeerPackets > (uint64)UINT32_MAX)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	State = *pState;
	memcpy(
		State.LocalKexRemaining,
		pRules->Local,
		sizeof(State.LocalKexRemaining)
	);
	memcpy(
		State.PeerKexRemaining,
		pRules->Peer,
		sizeof(State.PeerKexRemaining)
	);
	if ( bInitial ) {
		State.Strict = Features.Strict;
		State.AcceptExtInfo = Features.AcceptExtInfo;
		State.SendExtInfo = Features.SendExtInfo;
	}
	Code = xsshTransportConfigureGuess(
		&State,
		XSSH_TRANSPORT_LOCAL,
		bLocalSkip
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshTransportConfigureGuess(
		&State,
		XSSH_TRANSPORT_PEER,
		bPeerSkip
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	State.KexConfigured = true;
	*pState = State;
	return XSSH_OK;
}



/* 检查 KEX 方法消息或 guessed-packet。 */
static xsshcode xsshTransportKexMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	const uint8* pRemaining;

	if ( !xsshTransportDirectionKexActive(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( !pState->KexConfigured ) {
		return xsshTransportGuessPending(pState, Direction) ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( xsshTransportGuessPending(pState, Direction) ) {
		return XSSH_OK;
	}
	pRemaining = xsshTransportKexRemainingConst(pState, Direction);
	return pRemaining[(size_t)iMessage - XSSH_KEX_METHOD_MIN] != 0u ?
		XSSH_OK : xsshTransportDirectionError(Direction);
}



/* 检查普通消息的方向、KEX 范围和 EXT_INFO 时机。 */
xsshcode xrtSshTransportMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	bool bKexActive;
	bool bCanApplication;
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) &&
		(pState->Phase != XSSH_TRANSPORT_OPEN) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (iMessage == XSSH_MSG_KEXINIT) ||
		(iMessage == XSSH_MSG_NEWKEYS) ) {
		return xsshTransportDirectionError(Direction);
	}
	Code = xsshTransportPacketCheck(pState, Direction);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		return XSSH_OK;
	}
	if ( xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( iMessage == XSSH_MSG_EXT_INFO ) {
		return xsshTransportExtInfoCheck(pState, Direction);
	}
	bKexActive = xsshTransportDirectionKexActive(pState, Direction);
	bCanApplication = xsshTransportCanApplicationInternal(
		pState,
		Direction
	);
	if ( (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) ) {
		return bKexActive ?
			xsshTransportKexMessageCheck(pState, Direction, iMessage) :
			xsshTransportDirectionError(Direction);
	}
	if ( (iMessage >= 20u) && (iMessage <= 29u) ) {
		if ( bKexActive &&
			!(pState->Strict && (pState->KexCount == 0u)) ) {
			return XSSH_OK;
		}
		return xsshTransportDirectionError(Direction);
	}
	if ( (iMessage == XSSH_MSG_SERVICE_REQUEST) ||
		(iMessage == XSSH_MSG_SERVICE_ACCEPT) ||
		(iMessage == XSSH_MSG_NEWCOMPRESS) ) {
		return bCanApplication ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( bKexActive ) {
		if ( pState->Strict && (pState->KexCount == 0u) ) {
			return xsshTransportDirectionError(Direction);
		}
		return ((iMessage >= 1u) && (iMessage <= 19u)) ?
			XSSH_OK : xsshTransportDirectionError(Direction);
	}
	if ( bCanApplication ) {
		return XSSH_OK;
	}
	return ((iMessage >= 1u) && (iMessage <= 19u)) ?
		XSSH_OK : xsshTransportDirectionError(Direction);
}



/* 提交 KEX 方法消息并区分猜测包是否计入本代额度。 */
static void xsshTransportKexMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	bool bGuessPending = xsshTransportGuessPending(pState, Direction);
	bool bSkip = Direction == XSSH_TRANSPORT_LOCAL ?
		pState->LocalGuessSkip : pState->PeerGuessSkip;

	if ( bGuessPending ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pState->LocalGuessSeen = true;
			pState->LocalGuessMessage = iMessage;
		} else {
			pState->PeerGuessSeen = true;
			pState->PeerGuessMessage = iMessage;
		}
		if ( pState->KexConfigured && !bSkip ) {
			xsshTransportKexConsume(pState, Direction, iMessage);
		}
		return;
	}
	xsshTransportKexConsume(pState, Direction, iMessage);
}



/* 提交普通消息并推进机会窗口、猜测包与 packet 计数。 */
xsshcode xrtSshTransportMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
)
{
	xsshcode Code = xrtSshTransportMessageCheck(
		pState,
		Direction,
		iMessage
	);
	bool bKexMethod;

	if ( Code != XSSH_OK ) {
		return Code;
	}
	bKexMethod = (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) &&
		xsshTransportDirectionKexActive(pState, Direction);
	if ( (pState->KexCount == 0u) && !pState->KexConfigured &&
		!bKexMethod && (iMessage != XSSH_MSG_DISCONNECT) ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pState->LocalStrictViolation = true;
		} else {
			pState->PeerStrictViolation = true;
		}
	}
	if ( iMessage == XSSH_MSG_EXT_INFO ) {
		xsshTransportExtInfoCommit(pState, Direction);
	} else {
		xsshTransportFirstExtClose(pState, Direction);
	}
	if ( bKexMethod ) {
		xsshTransportKexMessageCommit(pState, Direction, iMessage);
	}
	xsshTransportPacketIncrement(pState, Direction);
	if ( iMessage == XSSH_MSG_DISCONNECT ) {
		pState->Phase = XSSH_TRANSPORT_CLOSING;
	}
	return XSSH_OK;
}



/* 检查 guessed packet 和全部 KEX 方法额度都已处理。 */
xsshcode xrtSshTransportNewKeysCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	xsshcode Code;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pState->Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!xsshTransportDirectionKexActive(pState, Direction) ||
		!pState->KexConfigured ||
		xsshTransportGuessPending(pState, Direction) ||
		!xsshTransportKexComplete(pState, Direction) ||
		xsshTransportAuthSuccessPending(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	if ( (pState->KexCount == UINT64_MAX) &&
		(Direction == XSSH_TRANSPORT_LOCAL ?
			pState->PeerNewKeys : pState->LocalNewKeys) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xsshTransportPacketCheck(pState, Direction);
	return Code;
}



/* 提交单方向 NEWKEYS，并在双方完成时结束本代 KEX。 */
xsshcode xrtSshTransportNewKeysCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint32* pActions
)
{
	xsshtransportstate State;
	uint32 iActions = XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS;
	bool bInitial;
	xsshcode Code;

	if ( !xrtMemRangeValid(pActions, sizeof(*pActions)) ||
		(xsshTransportStateValid(pState) && xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pActions,
			sizeof(*pActions)
		)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshTransportNewKeysCheck(pState, Direction);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	State = *pState;
	bInitial = State.KexCount == 0u;
	xsshTransportPacketIncrement(&State, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		State.LocalNewKeys = true;
		if ( bInitial && State.SendExtInfo ) {
			State.LocalFirstExtOpen = true;
		}
	} else {
		State.PeerNewKeys = true;
		if ( bInitial && State.AcceptExtInfo ) {
			State.PeerFirstExtOpen = true;
		}
	}
	if ( State.Strict ) {
		iActions |= XSSH_TRANSPORT_ACTION_RESET_SEQUENCE;
	}
	if ( State.LocalNewKeys && State.PeerNewKeys ) {
		State.KexCount++;
		State.Phase = XSSH_TRANSPORT_OPEN;
		iActions |= XSSH_TRANSPORT_ACTION_KEX_COMPLETE;
	}
	*pState = State;
	*pActions = iActions;
	return XSSH_OK;
}



/* 检查 server 认证成功方向和当前密钥边界。 */
xsshcode xrtSshTransportAuthSuccessCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	bool bServerDirection;

	if ( !xsshTransportStateValid(pState) ||
		!xsshTransportDirectionValid(Direction) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	bServerDirection = Direction == XSSH_TRANSPORT_LOCAL ?
		(pState->Role == XSSH_ROLE_SERVER) :
		(pState->Role == XSSH_ROLE_CLIENT);
	if ( !bServerDirection ||
		!xsshTransportCanApplicationInternal(pState, Direction) ||
		xsshTransportAuthSuccess(pState, Direction) ) {
		return xsshTransportDirectionError(Direction);
	}
	return xsshTransportPacketCheck(pState, Direction);
}



/* 提交 USERAUTH_SUCCESS，并完成第二次 EXT_INFO 的邻接约束。 */
xsshcode xrtSshTransportAuthSuccessCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
)
{
	xsshcode Code = xrtSshTransportAuthSuccessCheck(pState, Direction);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	xsshTransportFirstExtClose(pState, Direction);
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		pState->LocalAuthSuccessPending = false;
		pState->LocalAuthSuccess = true;
	} else {
		pState->PeerAuthSuccessPending = false;
		pState->PeerAuthSuccess = true;
	}
	xsshTransportPacketIncrement(pState, Direction);
	return XSSH_OK;
}



/* 标记承载连接已经关闭。 */
void xrtSshTransportClose(xsshtransportstate* pState)
{
	if ( xsshTransportStateValid(pState) ) {
		pState->Phase = XSSH_TRANSPORT_CLOSED;
	}
}

#endif
