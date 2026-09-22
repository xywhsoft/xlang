#include <string.h>

#include <xrt/ssh_kex_exchange.h>



#if defined(XSSH_FEATURE_KEX_EXCHANGE)

#define XSSH_KEX_EXCHANGE_GUARD UINT32_C(0x4b455845)



/* 校验角色和 transport 方向。 */
static bool xsshKexExchangeRoleValid(
	xsshrole Role,
	xsshtransportdirection Direction
)
{
	return ((Role == XSSH_ROLE_CLIENT) || (Role == XSSH_ROLE_SERVER)) &&
		((Direction == XSSH_TRANSPORT_LOCAL) ||
		 (Direction == XSSH_TRANSPORT_PEER));
}



/* 校验对象哨兵、阶段、事务类型和全部缓冲均无尾部预留。 */
static bool xsshKexExchangeValid(const xsshkexexchange* pExchange)
{
	return xrtMemRangeValid(pExchange, sizeof(*pExchange)) &&
		(pExchange->Guard == XSSH_KEX_EXCHANGE_GUARD) &&
		pExchange->Initialized &&
		xsshKexExchangeRoleValid(
			pExchange->Role,
			pExchange->PendingDirection
		) && (pExchange->Session.Role == pExchange->Role) &&
		(pExchange->Phase >= XSSH_KEX_EXCHANGE_IDENTIFICATION) &&
		(pExchange->Phase <= XSSH_KEX_EXCHANGE_FAILED) &&
		(pExchange->Pending >= XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		(pExchange->Pending <= XSSH_KEX_EXCHANGE_PENDING_KEXINIT) &&
		(pExchange->ClientVersion.Reserved == NULL) &&
		(pExchange->ServerVersion.Reserved == NULL) &&
		(pExchange->ClientKexInit.Reserved == NULL) &&
		(pExchange->ServerKexInit.Reserved == NULL) &&
		(pExchange->NextClientKexInit.Reserved == NULL) &&
		(pExchange->NextServerKexInit.Reserved == NULL) &&
		(pExchange->Staging.Reserved == NULL);
}



/* 校验对象没有未决保存事务。 */
static bool xsshKexExchangeStable(const xsshkexexchange* pExchange)
{
	return xsshKexExchangeValid(pExchange) &&
		(pExchange->Pending == XSSH_KEX_EXCHANGE_PENDING_NONE) &&
		xrtNetBufEmpty(&pExchange->Staging);
}



/* 校验 transport core 与交换对象互不重叠且角色一致。 */
static bool xsshKexExchangeCoreValid(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
			pCore,
			sizeof(*pCore)
		) && (pCore->State.Role == pExchange->Role);
}



/* 判断一个 local/peer 方向在连接两端对应 client 还是 server。 */
static bool xsshKexExchangeIsClient(
	const xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return (pExchange->Role == XSSH_ROLE_CLIENT) ==
		(Direction == XSSH_TRANSPORT_LOCAL);
}



/* 返回连接级 client/server 版本缓冲。 */
static xnetbuf* xsshKexExchangeVersionBuffer(
	xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return xsshKexExchangeIsClient(pExchange, Direction) ?
		&pExchange->ClientVersion : &pExchange->ServerVersion;
}



/* 返回下一代 client/server KEXINIT 缓冲。 */
static xnetbuf* xsshKexExchangeNextBuffer(
	xsshkexexchange* pExchange,
	xsshtransportdirection Direction
)
{
	return xsshKexExchangeIsClient(pExchange, Direction) ?
		&pExchange->NextClientKexInit :
		&pExchange->NextServerKexInit;
}



/* 返回方向对应的累计 packet 数。 */
static uint64 xsshKexExchangePackets(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalPackets : pCore->State.PeerPackets;
}



/* 返回方向对应的本代 KEXINIT 序号。 */
static uint64 xsshKexExchangeOrdinal(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalKexInitOrdinal :
		pCore->State.PeerKexInitOrdinal;
}



/* 返回方向是否已经提交本代 KEXINIT。 */
static bool xsshKexExchangeKexInitCommitted(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalKexInit : pCore->State.PeerKexInit;
}



/* 返回方向记录的 guessed-packet 标志。 */
static bool xsshKexExchangeGuessExpected(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalGuessExpected :
		pCore->State.PeerGuessExpected;
}



/* 返回方向对应的 identification 提交位。 */
static bool xsshKexExchangeIdentificationCommitted(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		pCore->State.LocalIdentification :
		pCore->State.PeerIdentification;
}



/* 返回与方向对应的 transport 未决 packet。 */
static const xsshtransportpending* xsshKexExchangeCorePending(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	return Direction == XSSH_TRANSPORT_LOCAL ?
		&pCore->Write : &pCore->Read;
}



/* 清除保存事务元数据，不触碰任何动态块。 */
static void xsshKexExchangePendingClear(xsshkexexchange* pExchange)
{
	pExchange->PendingOrdinal = 0u;
	pExchange->PendingKexCount = 0u;
	pExchange->PendingDirection = XSSH_TRANSPORT_LOCAL;
	pExchange->PendingCorePhase = XSSH_TRANSPORT_IDENTIFICATION;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_NONE;
	pExchange->PendingFirstKexPacketFollows = false;
}



/* 校验本端或对端无换行 identification。 */
static xsshcode xsshKexExchangeVersionValidate(
	xsshtransportdirection Direction,
	xstrview Version
)
{
	char arrLine[XSSH_IDENTIFICATION_MAX];
	xsshwriter Writer;
	xstrview Parsed;
	size_t iConsumed;
	xsshcode Code;

	if ( !xrtMemRangeValid(Version.Data, Version.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( !xrtSshWriterInit(&Writer, arrLine, sizeof(arrLine)) ) {
			return XSSH_ERROR_STATE;
		}
		return xrtSshBannerWrite(&Writer, Version);
	}
	if ( Version.Size == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Version.Size >= sizeof(arrLine) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	memcpy(arrLine, Version.Data, Version.Size);
	arrLine[Version.Size] = '\n';
	Code = xrtSshBannerRead(
		(xstrview){ arrLine, Version.Size + 1u },
		&Parsed,
		&iConsumed
	);
	if ( (Code == XSSH_OK) &&
		((Parsed.Size != Version.Size) ||
		 (iConsumed != (Version.Size + 1u)) ||
		 (memcmp(Parsed.Data, Version.Data, Version.Size) != 0)) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return Code;
}



/* 将一个非空动态链连续化为稳定借用视图。 */
static bool xsshKexExchangeView(xnetbuf* pBuffer, xbytesview* pView)
{
	xnetspan Span;
	size_t iSize = xrtNetBufSize(pBuffer);

	if ( (iSize == 0u) || !xrtNetBufPullup(pBuffer, iSize, &Span) ||
		(Span.Size < iSize) ) {
		return false;
	}
	pView->Data = Span.Data;
	pView->Size = iSize;
	return true;
}



/* 选择 READY 的下一代或 METHOD/COMPLETE 的当前代 KEXINIT。 */
static bool xsshKexExchangeTranscriptBuffers(
	xsshkexexchange* pExchange,
	xnetbuf** ppClient,
	xnetbuf** ppServer
)
{
	if ( pExchange->Phase == XSSH_KEX_EXCHANGE_READY ) {
		*ppClient = &pExchange->NextClientKexInit;
		*ppServer = &pExchange->NextServerKexInit;
		return true;
	}
	if ( (pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		(pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE) ) {
		*ppClient = &pExchange->ClientKexInit;
		*ppServer = &pExchange->ServerKexInit;
		return true;
	}
	return false;
}



/* 初始化动态 transcript 与可重复 rekey 的 KEX 会话。 */
bool xrtSshKexExchangeInit(
	xsshkexexchange* pExchange,
	xnetbufpool* pPool,
	xsshrole Role
)
{
	xsshkexexchange Exchange;

	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Exchange, 0, sizeof(Exchange));
	if ( !xrtSshKexSessionInit(&Exchange.Session, Role) ||
		!xrtNetBufInit(&Exchange.ClientVersion, pPool) ||
		!xrtNetBufInit(&Exchange.ServerVersion, pPool) ||
		!xrtNetBufInit(&Exchange.ClientKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.ServerKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.NextClientKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.NextServerKexInit, pPool) ||
		!xrtNetBufInit(&Exchange.Staging, pPool) ) {
		xrtSshKexSessionClear(&Exchange.Session);
		return false;
	}
	Exchange.Role = Role;
	Exchange.PendingDirection = XSSH_TRANSPORT_LOCAL;
	Exchange.PendingCorePhase = XSSH_TRANSPORT_IDENTIFICATION;
	Exchange.Phase = XSSH_KEX_EXCHANGE_IDENTIFICATION;
	Exchange.Initialized = true;
	Exchange.Guard = XSSH_KEX_EXCHANGE_GUARD;
	*pExchange = Exchange;
	return true;
}



/* 释放 transcript 并安全清除 KEX 会话。 */
void xrtSshKexExchangeClear(xsshkexexchange* pExchange)
{
	if ( xsshKexExchangeValid(pExchange) ) {
		xrtNetBufClear(&pExchange->ClientVersion);
		xrtNetBufClear(&pExchange->ServerVersion);
		xrtNetBufClear(&pExchange->ClientKexInit);
		xrtNetBufClear(&pExchange->ServerKexInit);
		xrtNetBufClear(&pExchange->NextClientKexInit);
		xrtNetBufClear(&pExchange->NextServerKexInit);
		xrtNetBufClear(&pExchange->Staging);
		xrtSshKexSessionClear(&pExchange->Session);
	}
	if ( xrtMemRangeValid(pExchange, sizeof(*pExchange)) ) {
		memset(pExchange, 0, sizeof(*pExchange));
	}
}



/* 在 transport 提交前保存经过 wire 规则校验的版本串。 */
xsshcode xrtSshKexExchangeVersionPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
)
{
	xnetbuf* pTarget;
	xsshcode Code;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeRoleValid(pExchange->Role, Direction) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pExchange->Phase != XSSH_KEX_EXCHANGE_IDENTIFICATION) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xrtMemRangesOverlap(
		pExchange,
		sizeof(*pExchange),
		Version.Data,
		Version.Size
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		Version.Data,
		Version.Size
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	pTarget = xsshKexExchangeVersionBuffer(pExchange, Direction);
	if ( !xrtNetBufEmpty(pTarget) ||
		xsshKexExchangeIdentificationCommitted(pCore, Direction) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xsshKexExchangeVersionValidate(Direction, Version);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtNetBufAppend(
		&pExchange->Staging,
		Version.Data,
		Version.Size
	) ) {
		return XSSH_ERROR_SPACE;
	}
	pExchange->PendingDirection = Direction;
	pExchange->PendingCorePhase = pCore->State.Phase;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_VERSION;
	return XSSH_OK;
}



/* 在 core 提交后发布稳定版本串。 */
xsshcode xrtSshKexExchangeVersionCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	xnetbuf* pTarget;

	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		!xsshKexExchangeIdentificationCommitted(
			pCore,
			pExchange->PendingDirection
		) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshKexExchangeVersionBuffer(
		pExchange,
		pExchange->PendingDirection
	);
	if ( !xrtNetBufEmpty(pTarget) ||
		!xrtNetBufMove(pTarget, &pExchange->Staging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexExchangePendingClear(pExchange);
	if ( !xrtNetBufEmpty(&pExchange->ClientVersion) &&
		!xrtNetBufEmpty(&pExchange->ServerVersion) ) {
		pExchange->Phase = XSSH_KEX_EXCHANGE_KEXINIT;
	}
	return XSSH_OK;
}



/* 在 core 尚未提交 identification 时取消保存。 */
xsshcode xrtSshKexExchangeVersionAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_VERSION) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		xsshKexExchangeIdentificationCommitted(
			pCore,
			pExchange->PendingDirection
		) || (pCore->State.Phase != pExchange->PendingCorePhase) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pExchange->Staging);
	xsshKexExchangePendingClear(pExchange);
	return XSSH_OK;
}



/* 本端在 core 写事务前保存 KEXINIT，对端关联已经认证的 core 读事务。 */
xsshcode xrtSshKexExchangeKexInitPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xbytesview Payload
)
{
	xsshkexinit KexInit;
	xnetbuf* pTarget;
	xsshcode Code;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeRoleValid(pExchange->Role, Direction) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		((pExchange->Phase != XSSH_KEX_EXCHANGE_KEXINIT) &&
		 (pExchange->Phase != XSSH_KEX_EXCHANGE_COMPLETE)) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
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
	pTarget = xsshKexExchangeNextBuffer(pExchange, Direction);
	if ( !xrtNetBufEmpty(pTarget) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshKexInitRead(Payload, &KexInit);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (Direction == XSSH_TRANSPORT_LOCAL) &&
		KexInit.FirstKexPacketFollows ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	if ( Direction == XSSH_TRANSPORT_LOCAL ) {
		if ( pCore->Write.Active || pCore->Read.Active ||
			(xrtSshTransportKexInitCheck(
				&pCore->State,
				XSSH_TRANSPORT_LOCAL
			) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	} else if ( !pCore->Read.Active || pCore->Write.Active ||
		(pCore->Read.Kind != XSSH_TRANSPORT_PACKET_KEXINIT) ||
		(pCore->Read.Message != XSSH_MSG_KEXINIT) ||
		(pCore->Read.FirstKexPacketFollows !=
		 KexInit.FirstKexPacketFollows) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtNetBufAppend(
		&pExchange->Staging,
		Payload.Data,
		Payload.Size
	) ) {
		return XSSH_ERROR_SPACE;
	}
	pExchange->PendingOrdinal = xsshKexExchangePackets(
		pCore,
		Direction
	);
	pExchange->PendingKexCount = pCore->State.KexCount;
	pExchange->PendingDirection = Direction;
	pExchange->PendingCorePhase = pCore->State.Phase;
	pExchange->Pending = XSSH_KEX_EXCHANGE_PENDING_KEXINIT;
	pExchange->PendingFirstKexPacketFollows =
		KexInit.FirstKexPacketFollows;
	return XSSH_OK;
}



/* 只在 core 已提交同一 packet 序号后发布下一代 KEXINIT。 */
xsshcode xrtSshKexExchangeKexInitCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	const xsshtransportpending* pPending;
	xnetbuf* pTarget;
	uint64 iPackets;

	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_KEXINIT) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pCore->State.KexCount != pExchange->PendingKexCount) ||
		(pExchange->PendingOrdinal == UINT64_MAX) ) {
		return XSSH_ERROR_STATE;
	}
	pPending = xsshKexExchangeCorePending(
		pCore,
		pExchange->PendingDirection
	);
	iPackets = xsshKexExchangePackets(
		pCore,
		pExchange->PendingDirection
	);
	if ( pPending->Active ||
		(pCore->State.Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!xsshKexExchangeKexInitCommitted(
			pCore,
			pExchange->PendingDirection
		) || (xsshKexExchangeOrdinal(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingOrdinal) ||
		(iPackets != (pExchange->PendingOrdinal + 1u)) ||
		(xsshKexExchangeGuessExpected(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingFirstKexPacketFollows) ) {
		return XSSH_ERROR_STATE;
	}
	pTarget = xsshKexExchangeNextBuffer(
		pExchange,
		pExchange->PendingDirection
	);
	if ( !xrtNetBufEmpty(pTarget) ||
		!xrtNetBufMove(pTarget, &pExchange->Staging) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexExchangePendingClear(pExchange);
	pExchange->Phase = !xrtNetBufEmpty(
		&pExchange->NextClientKexInit
	) && !xrtNetBufEmpty(
		&pExchange->NextServerKexInit
	) ? XSSH_KEX_EXCHANGE_READY : XSSH_KEX_EXCHANGE_KEXINIT;
	return XSSH_OK;
}



/* packet 计数和 KEX 代际未推进时取消暂存副本。 */
xsshcode xrtSshKexExchangeKexInitAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeValid(pExchange) ||
		(pExchange->Pending != XSSH_KEX_EXCHANGE_PENDING_KEXINIT) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pCore->State.KexCount != pExchange->PendingKexCount) ||
		(pCore->State.Phase != pExchange->PendingCorePhase) ||
		(xsshKexExchangePackets(
			pCore,
			pExchange->PendingDirection
		) != pExchange->PendingOrdinal) ) {
		return XSSH_ERROR_STATE;
	}
	xrtNetBufClear(&pExchange->Staging);
	xsshKexExchangePendingClear(pExchange);
	return XSSH_OK;
}



/* 校验动态 transcript 和 transport 已到方法交换边界。 */
bool xrtSshKexExchangeReady(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	return xsshKexExchangeStable(pExchange) &&
		xsshKexExchangeCoreValid(pExchange, pCore) &&
		(pExchange->Phase == XSSH_KEX_EXCHANGE_READY) &&
		!xrtNetBufEmpty(&pExchange->ClientVersion) &&
		!xrtNetBufEmpty(&pExchange->ServerVersion) &&
		!xrtNetBufEmpty(&pExchange->NextClientKexInit) &&
		!xrtNetBufEmpty(&pExchange->NextServerKexInit) &&
		(pCore->State.Phase == XSSH_TRANSPORT_KEY_EXCHANGE) &&
		pCore->State.LocalKexInit && pCore->State.PeerKexInit &&
		!pCore->State.KexConfigured && !pCore->Write.Active &&
		!pCore->Read.Active;
}



/* 连续化四条动态链并返回不含 packet framing 的 transcript。 */
xsshcode xrtSshKexExchangeTranscript(
	xsshkexexchange* pExchange,
	xsshkextranscript* pTranscript
)
{
	xsshkextranscript Transcript;
	xnetbuf* pClient;
	xnetbuf* pServer;

	if ( !xsshKexExchangeStable(pExchange) ||
		!xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) ||
		xrtMemRangesOverlap(
			pExchange,
			sizeof(*pExchange),
			pTranscript,
			sizeof(*pTranscript)
		) || !xsshKexExchangeTranscriptBuffers(
			pExchange,
			&pClient,
			&pServer
		) ) {
		return XSSH_ERROR_STATE;
	}
	memset(&Transcript, 0, sizeof(Transcript));
	if ( !xsshKexExchangeView(
		&pExchange->ClientVersion,
		&Transcript.ClientVersion
	) || !xsshKexExchangeView(
		&pExchange->ServerVersion,
		&Transcript.ServerVersion
	) || !xsshKexExchangeView(
		pClient,
		&Transcript.ClientKexInit
	) || !xsshKexExchangeView(
		pServer,
		&Transcript.ServerKexInit
	) ) {
		return XSSH_ERROR_SPACE;
	}
	return xrtSshKexTranscriptInit(
		pTranscript,
		Transcript.ClientVersion,
		Transcript.ServerVersion,
		Transcript.ClientKexInit,
		Transcript.ServerKexInit
	);
}



/* 配置本代 KEX，并在成功后释放旧代动态块。 */
xsshcode xrtSshKexExchangeBeginWithPrivate(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	xsshkextranscript Transcript;
	xsshcode Code;

	if ( !xrtSshKexExchangeReady(pExchange, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshKexExchangeTranscript(pExchange, &Transcript);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexSessionBeginWithPrivate(
			&pExchange->Session,
			pCore,
			&Transcript,
			ServerHostKey,
			PrivateKey
		);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtNetBufClear(&pExchange->ClientKexInit);
	xrtNetBufClear(&pExchange->ServerKexInit);
	if ( !xrtNetBufMove(
		&pExchange->ClientKexInit,
		&pExchange->NextClientKexInit
	) || !xrtNetBufMove(
		&pExchange->ServerKexInit,
		&pExchange->NextServerKexInit
	) ) {
		xrtSshKexExchangeFail(pExchange);
		xrtSshTransportCoreClose(pCore);
		return XSSH_ERROR_STATE;
	}
	pExchange->Phase = XSSH_KEX_EXCHANGE_METHOD;
	return XSSH_OK;
}



/* 借出可推进的 KEX 会话。 */
xsshkexsession* xrtSshKexExchangeSession(xsshkexexchange* pExchange)
{
	return xsshKexExchangeStable(pExchange) &&
		((pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		 (pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE)) ?
		&pExchange->Session : NULL;
}



/* 借出只读 KEX 会话。 */
const xsshkexsession* xrtSshKexExchangeSessionConst(
	const xsshkexexchange* pExchange
)
{
	return xsshKexExchangeStable(pExchange) &&
		((pExchange->Phase == XSSH_KEX_EXCHANGE_METHOD) ||
		 (pExchange->Phase == XSSH_KEX_EXCHANGE_COMPLETE)) ?
		&pExchange->Session : NULL;
}



/* 确认会话与 transport 都完成本代密钥切换。 */
xsshcode xrtSshKexExchangeComplete(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
)
{
	if ( !xsshKexExchangeStable(pExchange) ||
		!xsshKexExchangeCoreValid(pExchange, pCore) ||
		(pExchange->Phase != XSSH_KEX_EXCHANGE_METHOD) ||
		!xrtSshKexSessionComplete(&pExchange->Session, pCore) ) {
		return XSSH_ERROR_STATE;
	}
	pExchange->Phase = XSSH_KEX_EXCHANGE_COMPLETE;
	return XSSH_OK;
}



/* 终止交换并释放尚未晋升的材料。 */
void xrtSshKexExchangeFail(xsshkexexchange* pExchange)
{
	if ( xsshKexExchangeValid(pExchange) ) {
		xrtNetBufClear(&pExchange->NextClientKexInit);
		xrtNetBufClear(&pExchange->NextServerKexInit);
		xrtNetBufClear(&pExchange->Staging);
		xrtSshKexSessionFail(&pExchange->Session);
		xsshKexExchangePendingClear(pExchange);
		pExchange->Phase = XSSH_KEX_EXCHANGE_FAILED;
	}
}

#endif
