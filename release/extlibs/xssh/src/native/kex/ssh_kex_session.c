#include <xrt/ssh_kex_session.h>
#include <string.h>



#if defined(XSSH_FEATURE_KEX_SESSION)

#define XSSH_KEX_SESSION_GUARD UINT32_C(0x4B455853)



/* 校验会话对象的结构哨兵和角色。 */
static bool xsshKexSessionValid(const xsshkexsession* pSession)
{
	return xrtMemRangeValid(pSession, sizeof(*pSession)) &&
		(pSession->Guard == XSSH_KEX_SESSION_GUARD) &&
		((pSession->Role == XSSH_ROLE_CLIENT) ||
		 (pSession->Role == XSSH_ROLE_SERVER));
}



/* 校验 SSH identification 中参与 exchange hash 的无换行版本串。 */
static bool xsshKexVersionValid(xbytesview Version)
{
	char arrLine[XSSH_IDENTIFICATION_MAX];
	xstrview Banner;
	size_t iConsumed;

	if ( !xrtMemRangeValid(Version.Data, Version.Size) ||
		(Version.Size < 8u) ||
		(Version.Size >= XSSH_IDENTIFICATION_MAX) ) {
		return false;
	}
	memcpy(arrLine, Version.Data, Version.Size);
	arrLine[Version.Size] = '\n';
	return (xrtSshBannerRead(
		(xstrview){ arrLine, Version.Size + 1u },
		&Banner,
		&iConsumed
	) == XSSH_OK) && (Banner.Size == Version.Size) &&
		(iConsumed == (Version.Size + 1u)) &&
		(memcmp(Banner.Data, Version.Data, Version.Size) == 0);
}



/* 校验 transcript 及四段借用输入。 */
static bool xsshKexTranscriptValid(const xsshkextranscript* pTranscript)
{
	return xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) &&
		xsshKexVersionValid(pTranscript->ClientVersion) &&
		xsshKexVersionValid(pTranscript->ServerVersion) &&
		xrtMemRangeValid(
			pTranscript->ClientKexInit.Data,
			pTranscript->ClientKexInit.Size
		) && xrtMemRangeValid(
			pTranscript->ServerKexInit.Data,
			pTranscript->ServerKexInit.Size
		) && (pTranscript->ClientKexInit.Size != 0u) &&
		(pTranscript->ServerKexInit.Size != 0u);
}



/* 判断对象范围是否与 transcript 任一借用字节段重叠。 */
static bool xsshKexTranscriptOverlaps(
	const xsshkextranscript* pTranscript,
	const void* pData,
	size_t iSize
)
{
	return xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ClientVersion.Data,
		pTranscript->ClientVersion.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ServerVersion.Data,
		pTranscript->ServerVersion.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ClientKexInit.Data,
		pTranscript->ClientKexInit.Size
	) || xrtMemRangesOverlap(
		pData,
		iSize,
		pTranscript->ServerKexInit.Data,
		pTranscript->ServerKexInit.Size
	);
}



/* 比较借用文本与常量算法名。 */
static bool xsshKexTextEqual(xstrview Text, const char* pExpected)
{
	size_t iSize = strlen(pExpected);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, pExpected, iSize) == 0);
}



/* 返回当前实现支持的 AES-GCM 密钥长度。 */
static xsshcode xsshKexCipherKeySize(xstrview Cipher, uint8* pSize)
{
	if ( xsshKexTextEqual(Cipher, "aes128-gcm@openssh.com") ) {
		*pSize = 16u;
		return XSSH_OK;
	}
	if ( xsshKexTextEqual(Cipher, "aes256-gcm@openssh.com") ) {
		*pSize = 32u;
		return XSSH_OK;
	}
	return XSSH_ERROR_UNSUPPORTED;
}



/* 验证本会话能够直接驱动的算法组合。 */
static xsshcode xsshKexNegotiationSupported(
	const xsshkexnegotiation* pNegotiation,
	uint8* pClientToServerKeySize,
	uint8* pServerToClientKeySize
)
{
	xsshcode Code;

	if ( (!xsshKexTextEqual(
		pNegotiation->KexAlgorithm,
		"curve25519-sha256"
	) && !xsshKexTextEqual(
		pNegotiation->KexAlgorithm,
		"curve25519-sha256@libssh.org"
	)) || !xsshKexTextEqual(
		pNegotiation->ServerHostKeyAlgorithm,
		XSSH_HOSTKEY_ED25519
	) || !xsshKexTextEqual(
		pNegotiation->CompressionClientToServer,
		"none"
	) || !xsshKexTextEqual(
		pNegotiation->CompressionServerToClient,
		"none"
	) ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	Code = xsshKexCipherKeySize(
		pNegotiation->CipherClientToServer,
		pClientToServerKeySize
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xsshKexCipherKeySize(
		pNegotiation->CipherServerToClient,
		pServerToClientKeySize
	);
}



/* 将会话阶段同步到已经提交的 KEX 状态。 */
static void xsshKexSessionUpdatePhase(xsshkexsession* pSession)
{
	if ( !pSession->Active ||
		(pSession->Phase == XSSH_KEX_SESSION_FAILED) ) {
		return;
	}
	if ( pSession->WriteActivated && pSession->ReadActivated ) {
		pSession->Phase = XSSH_KEX_SESSION_COMPLETE;
		return;
	}
	if ( (pSession->Role == XSSH_ROLE_CLIENT) &&
		pSession->MethodReadCommitted &&
		!pSession->HostKeyAccepted ) {
		pSession->Phase = XSSH_KEX_SESSION_HOST_KEY;
		return;
	}
	if ( pSession->MethodWriteCommitted &&
		pSession->MethodReadCommitted &&
		((pSession->Role == XSSH_ROLE_SERVER) ||
		 pSession->HostKeyAccepted) ) {
		pSession->Phase = XSSH_KEX_SESSION_NEW_KEYS;
		return;
	}
	pSession->Phase = XSSH_KEX_SESSION_METHOD;
}



/* 发生不可恢复错误时清除本代及跨代秘密。 */
static void xsshKexSessionSetFailed(xsshkexsession* pSession)
{
	xsshrole Role = pSession->Role;

	xrtSecureZero(pSession, sizeof(*pSession));
	pSession->Role = Role;
	pSession->Phase = XSSH_KEX_SESSION_FAILED;
	pSession->Guard = XSSH_KEX_SESSION_GUARD;
}



/* 计算 exchange hash 并派生本代双向 AES-GCM 材料。 */
static xsshcode xsshKexSessionDerive(xsshkexsession* pSession)
{
	xsshkexhashsha256 Input;
	xbytesview ClientPublic;
	xbytesview ServerPublic;
	xbytesview Shared;
	xbytesview Hash;
	xbytesview SessionId;
	xsshcode Code;

	ClientPublic = pSession->Role == XSSH_ROLE_CLIENT ?
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) } :
		(xbytesview){ pSession->PeerPublicKey, sizeof(pSession->PeerPublicKey) };
	ServerPublic = pSession->Role == XSSH_ROLE_SERVER ?
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) } :
		(xbytesview){ pSession->PeerPublicKey, sizeof(pSession->PeerPublicKey) };
	Shared = (xbytesview){
		pSession->SharedSecret,
		sizeof(pSession->SharedSecret)
	};
	memset(&Input, 0, sizeof(Input));
	Input.ClientVersion = pSession->Transcript.ClientVersion;
	Input.ServerVersion = pSession->Transcript.ServerVersion;
	Input.ClientKexInit = pSession->Transcript.ClientKexInit;
	Input.ServerKexInit = pSession->Transcript.ServerKexInit;
	Input.ServerHostKey = pSession->ServerHostKey;
	Input.ClientEphemeral = ClientPublic;
	Input.ServerEphemeral = ServerPublic;
	Input.SharedSecret = Shared;
	Code = xrtSshKexHashSha256(&Input, pSession->ExchangeHash);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !pSession->HasSessionId ) {
		memcpy(
			pSession->SessionId,
			pSession->ExchangeHash,
			sizeof(pSession->SessionId)
		);
		pSession->HasSessionId = true;
	}
	Hash = (xbytesview){
		pSession->ExchangeHash,
		sizeof(pSession->ExchangeHash)
	};
	SessionId = (xbytesview){
		pSession->SessionId,
		sizeof(pSession->SessionId)
	};
	Code = xrtSshKexDeriveSha256(
		pSession->ClientToServerIV,
		sizeof(pSession->ClientToServerIV),
		Shared,
		pSession->ExchangeHash,
		pSession->SessionId,
		(uint8)'A'
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV),
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'B'
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize,
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'C'
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexDeriveSha256(
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize,
			Shared,
			Hash.Data,
			SessionId.Data,
			(uint8)'D'
		);
	}
	if ( Code == XSSH_OK ) {
		pSession->KeysDerived = true;
	} else {
		xrtSecureZero(
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		);
		xrtSecureZero(
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		);
		xrtSecureZero(
			pSession->ClientToServerKey,
			sizeof(pSession->ClientToServerKey)
		);
		xrtSecureZero(
			pSession->ServerToClientKey,
			sizeof(pSession->ServerToClientKey)
		);
	}
	return Code;
}



/* 判断当前 peer 方法包是否是协商后必须丢弃的猜测包。 */
static bool xsshKexSessionDiscardGuess(
	const xsshtransportcore* pCore,
	uint8 iMessage
)
{
	return (iMessage >= XSSH_KEX_METHOD_MIN) &&
		(iMessage <= XSSH_KEX_METHOD_MAX) &&
		pCore->State.PeerGuessExpected &&
		!pCore->State.PeerGuessSeen &&
		pCore->State.PeerGuessSkip;
}



/* 校验当前 payload 确实来自 core 的未提交读事务。 */
static bool xsshKexSessionCoreReadMatches(
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint8 iMessage
)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		pCore->Read.Active &&
		(pCore->Read.Message == iMessage) &&
		(Payload.Size != 0u) && (Payload.Data[0] == iMessage);
}



/* 初始化四段借用 transcript。 */
xsshcode xrtSshKexTranscriptInit(
	xsshkextranscript* pTranscript,
	xbytesview ClientVersion,
	xbytesview ServerVersion,
	xbytesview ClientKexInit,
	xbytesview ServerKexInit
)
{
	xsshkextranscript Transcript;

	if ( !xrtMemRangeValid(pTranscript, sizeof(*pTranscript)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Transcript.ClientVersion = ClientVersion;
	Transcript.ServerVersion = ServerVersion;
	Transcript.ClientKexInit = ClientKexInit;
	Transcript.ServerKexInit = ServerKexInit;
	if ( !xsshKexTranscriptValid(&Transcript) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xsshKexTranscriptOverlaps(
		&Transcript,
		pTranscript,
		sizeof(*pTranscript)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pTranscript = Transcript;
	return XSSH_OK;
}



/* 计算 transcript 四段原始字节总量。 */
xsshcode xrtSshKexTranscriptMeasure(
	const xsshkextranscript* pTranscript,
	size_t* pSize
)
{
	size_t iSize;

	if ( !xsshKexTranscriptValid(pTranscript) ||
		!xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		xrtMemRangesOverlap(
			pTranscript,
			sizeof(*pTranscript),
			pSize,
			sizeof(*pSize)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pSize,
			sizeof(*pSize)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iSize = pTranscript->ClientVersion.Size;
	if ( (pTranscript->ServerVersion.Size > (SIZE_MAX - iSize)) ||
		(pTranscript->ClientKexInit.Size >
		 (SIZE_MAX - iSize - pTranscript->ServerVersion.Size)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iSize += pTranscript->ServerVersion.Size;
	iSize += pTranscript->ClientKexInit.Size;
	if ( pTranscript->ServerKexInit.Size > (SIZE_MAX - iSize) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize = iSize + pTranscript->ServerKexInit.Size;
	return XSSH_OK;
}



/* 一次预留后复制 transcript，保证失败不推进 writer。 */
xsshcode xrtSshKexTranscriptWrite(
	xsshwriter* pWriter,
	const xsshkextranscript* pInput,
	xsshkextranscript* pOutput
)
{
	xsshkextranscript Input;
	xbytesview arrInputs[4];
	xsshkextranscript Output;
	size_t iSize;
	size_t iStart;
	xsshcode Code;

	if ( !xsshKexTranscriptValid(pInput) ||
		!xrtMemRangeValid(pOutput, sizeof(*pOutput)) ||
		xsshKexTranscriptOverlaps(
			pInput,
			pOutput,
			sizeof(*pOutput)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Input = *pInput;
	Code = xrtSshKexTranscriptMeasure(&Input, &iSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	arrInputs[0] = Input.ClientVersion;
	arrInputs[1] = Input.ServerVersion;
	arrInputs[2] = Input.ClientKexInit;
	arrInputs[3] = Input.ServerKexInit;
	Code = xrtSshWriterReserveInputs(pWriter, iSize, arrInputs, 4u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		pOutput,
		sizeof(*pOutput),
		pWriter->Data + pWriter->Size,
		iSize
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	iStart = pWriter->Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ClientVersion.Data,
		Input.ClientVersion.Size);
	pWriter->Size += Input.ClientVersion.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ServerVersion.Data,
		Input.ServerVersion.Size);
	pWriter->Size += Input.ServerVersion.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ClientKexInit.Data,
		Input.ClientKexInit.Size);
	pWriter->Size += Input.ClientKexInit.Size;
	memcpy(pWriter->Data + pWriter->Size, Input.ServerKexInit.Data,
		Input.ServerKexInit.Size);
	pWriter->Size += Input.ServerKexInit.Size;
	Output.ClientVersion = (xbytesview){
		pWriter->Data + iStart,
		Input.ClientVersion.Size
	};
	iStart += Input.ClientVersion.Size;
	Output.ServerVersion = (xbytesview){
		pWriter->Data + iStart,
		Input.ServerVersion.Size
	};
	iStart += Input.ServerVersion.Size;
	Output.ClientKexInit = (xbytesview){
		pWriter->Data + iStart,
		Input.ClientKexInit.Size
	};
	iStart += Input.ClientKexInit.Size;
	Output.ServerKexInit = (xbytesview){
		pWriter->Data + iStart,
		Input.ServerKexInit.Size
	};
	*pOutput = Output;
	return XSSH_OK;
}



/* 初始化无外部资源的 KEX 会话。 */
bool xrtSshKexSessionInit(xsshkexsession* pSession, xsshrole Role)
{
	xsshkexsession Session;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		((Role != XSSH_ROLE_CLIENT) && (Role != XSSH_ROLE_SERVER)) ) {
		return false;
	}
	memset(&Session, 0, sizeof(Session));
	Session.Role = Role;
	Session.Phase = XSSH_KEX_SESSION_IDLE;
	Session.Guard = XSSH_KEX_SESSION_GUARD;
	xrtSecureZero(pSession, sizeof(*pSession));
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return true;
}



/* 安全清除完整 KEX 会话。 */
void xrtSshKexSessionClear(xsshkexsession* pSession)
{
	if ( xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		xrtSecureZero(pSession, sizeof(*pSession));
	}
}



/* 配置一代确定性 Curve25519/Ed25519/AES-GCM KEX。 */
xsshcode xrtSshKexSessionBeginWithPrivate(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
)
{
	xsshkexsession Session;
	xsshkexinit ClientKex;
	xsshkexinit ServerKex;
	xsshkexinit* pLocal;
	xsshkexinit* pPeer;
	xsshtransportkexrules Rules;
	xbytesview PublicKey;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		!xsshKexTranscriptValid(pTranscript) ||
		!xrtMemRangeValid(PrivateKey.Data, PrivateKey.Size) ||
		(PrivateKey.Size != XSSH_CURVE25519_PRIVATE_SIZE) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pTranscript,
			sizeof(*pTranscript)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pTranscript,
			sizeof(*pTranscript)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pSession,
			sizeof(*pSession)
		) || xsshKexTranscriptOverlaps(
			pTranscript,
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			ServerHostKey.Data,
			ServerHostKey.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			ServerHostKey.Data,
			ServerHostKey.Size
		) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			PrivateKey.Data,
			PrivateKey.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			PrivateKey.Data,
			PrivateKey.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pSession->Active &&
		(pSession->Phase != XSSH_KEX_SESSION_COMPLETE) ) {
		return XSSH_ERROR_STATE;
	}
	if ( (pCore->State.Role != pSession->Role) ||
		(pCore->State.Phase != XSSH_TRANSPORT_KEY_EXCHANGE) ||
		!pCore->State.LocalKexInit || !pCore->State.PeerKexInit ||
		pCore->State.KexConfigured || pCore->Write.Active ||
		pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( ((pCore->State.KexCount == 0u) && pSession->HasSessionId) ||
		((pCore->State.KexCount != 0u) && !pSession->HasSessionId) ) {
		return XSSH_ERROR_STATE;
	}
	memset(&ClientKex, 0, sizeof(ClientKex));
	memset(&ServerKex, 0, sizeof(ServerKex));
	Code = xrtSshKexInitRead(pTranscript->ClientKexInit, &ClientKex);
	if ( Code == XSSH_OK ) {
		Code = xrtSshKexInitRead(pTranscript->ServerKexInit, &ServerKex);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	pLocal = pSession->Role == XSSH_ROLE_CLIENT ? &ClientKex : &ServerKex;
	pPeer = pSession->Role == XSSH_ROLE_CLIENT ? &ServerKex : &ClientKex;
	if ( pLocal->FirstKexPacketFollows ) {
		return XSSH_ERROR_UNSUPPORTED;
	}
	Session = *pSession;
	Session.Transcript = *pTranscript;
	memset(&Session.Negotiation, 0, sizeof(Session.Negotiation));
	Code = xrtSshKexNegotiate(
		&ClientKex,
		&ServerKex,
		&Session.Negotiation
	);
	if ( Code == XSSH_OK ) {
		Code = xsshKexNegotiationSupported(
			&Session.Negotiation,
			&Session.ClientToServerKeySize,
			&Session.ServerToClientKeySize
		);
	}
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	Session.ServerHostKey = (xbytesview){ NULL, 0u };
	if ( Session.Role == XSSH_ROLE_SERVER ) {
		Code = xrtSshEd25519PublicKeyRead(ServerHostKey, &PublicKey);
		if ( Code != XSSH_OK ) {
			xrtSecureZero(&Session, sizeof(Session));
			return Code;
		}
		Session.ServerHostKey = ServerHostKey;
		Session.HostKeyVerified = true;
		Session.HostKeyAccepted = true;
	} else if ( !xrtMemRangeValid(ServerHostKey.Data, ServerHostKey.Size) ||
		(ServerHostKey.Size != 0u) ) {
		xrtSecureZero(&Session, sizeof(Session));
		return XSSH_ERROR_ARGUMENT;
	}
	memcpy(Session.PrivateKey, PrivateKey.Data, sizeof(Session.PrivateKey));
	Code = xrtSshCurve25519Public(
		Session.PrivateKey,
		Session.PublicKey
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	Session.Active = true;
	Session.Phase = XSSH_KEX_SESSION_METHOD;
	Session.WritePending = XSSH_KEX_PACKET_NONE;
	Session.ReadPending = XSSH_KEX_PACKET_NONE;
	Session.KeysDerived = false;
	Session.MethodWriteCommitted = false;
	Session.MethodReadCommitted = false;
	Session.LocalNewKeys = false;
	Session.PeerNewKeys = false;
	Session.WriteActivated = false;
	Session.ReadActivated = false;
	if ( Session.Role == XSSH_ROLE_CLIENT ) {
		Session.HostKeyVerified = false;
		Session.HostKeyAccepted = false;
	}
	xrtSecureZero(Session.PeerPublicKey, sizeof(Session.PeerPublicKey));
	xrtSecureZero(Session.SharedSecret, sizeof(Session.SharedSecret));
	xrtSecureZero(Session.ExchangeHash, sizeof(Session.ExchangeHash));
	xrtSecureZero(Session.ClientToServerIV, sizeof(Session.ClientToServerIV));
	xrtSecureZero(Session.ServerToClientIV, sizeof(Session.ServerToClientIV));
	xrtSecureZero(Session.ClientToServerKey, sizeof(Session.ClientToServerKey));
	xrtSecureZero(Session.ServerToClientKey, sizeof(Session.ServerToClientKey));
	if ( !xrtSshTransportKexRulesInit(&Rules) ||
		!xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			Session.Role == XSSH_ROLE_CLIENT ?
				XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY,
			1u
		) || !xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			Session.Role == XSSH_ROLE_CLIENT ?
				XSSH_MSG_KEX_ECDH_REPLY : XSSH_MSG_KEX_ECDH_INIT,
			1u
		) ) {
		xrtSecureZero(&Session, sizeof(Session));
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshTransportCoreKexConfigure(
		pCore,
		pLocal,
		pPeer,
		&Session.Negotiation,
		&Rules
	);
	if ( Code != XSSH_OK ) {
		xrtSecureZero(&Session, sizeof(Session));
		return Code;
	}
	*pSession = Session;
	xrtSecureZero(&Session, sizeof(Session));
	return XSSH_OK;
}



/* 计算常见驱动的下一动作。 */
xsshkexsessionevent xrtSshKexSessionEvent(const xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ) {
		return XSSH_KEX_EVENT_NONE;
	}
	if ( pSession->Phase == XSSH_KEX_SESSION_FAILED ) {
		return XSSH_KEX_EVENT_FAILED;
	}
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		return XSSH_KEX_EVENT_COMPLETE;
	}
	if ( !pSession->Active ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ) {
		return XSSH_KEX_EVENT_NONE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		if ( !pSession->MethodWriteCommitted ) {
			return XSSH_KEX_EVENT_WRITE_ECDH_INIT;
		}
		if ( !pSession->MethodReadCommitted ) {
			return XSSH_KEX_EVENT_READ_ECDH_REPLY;
		}
		if ( !pSession->HostKeyAccepted ) {
			return XSSH_KEX_EVENT_VERIFY_HOST_KEY;
		}
	} else {
		if ( !pSession->MethodReadCommitted ) {
			return XSSH_KEX_EVENT_READ_ECDH_INIT;
		}
		if ( !pSession->MethodWriteCommitted ) {
			return XSSH_KEX_EVENT_WRITE_ECDH_REPLY;
		}
	}
	if ( !pSession->LocalNewKeys ) {
		return XSSH_KEX_EVENT_WRITE_NEWKEYS;
	}
	if ( !pSession->WriteActivated ) {
		return XSSH_KEX_EVENT_ACTIVATE_WRITE;
	}
	if ( !pSession->PeerNewKeys ) {
		return XSSH_KEX_EVENT_READ_NEWKEYS;
	}
	if ( !pSession->ReadActivated ) {
		return XSSH_KEX_EVENT_ACTIVATE_READ;
	}
	return XSSH_KEX_EVENT_COMPLETE;
}



/* 返回协商结果快照。 */
xsshcode xrtSshKexSessionNegotiation(
	const xsshkexsession* pSession,
	xsshkexnegotiation* pNegotiation
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		!xrtMemRangeValid(pNegotiation, sizeof(*pNegotiation)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pNegotiation,
			sizeof(*pNegotiation)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pNegotiation = pSession->Negotiation;
	return XSSH_OK;
}



/* 返回已计算的 exchange hash。 */
xsshcode xrtSshKexSessionExchangeHash(
	const xsshkexsession* pSession,
	xbytesview* pHash
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->KeysDerived ||
		!xrtMemRangeValid(pHash, sizeof(*pHash)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHash,
			sizeof(*pHash)
		) ) {
		return XSSH_ERROR_STATE;
	}
	pHash->Data = pSession->ExchangeHash;
	pHash->Size = sizeof(pSession->ExchangeHash);
	return XSSH_OK;
}



/* 返回跨 rekey 保持的 SessionId。 */
xsshcode xrtSshKexSessionId(
	const xsshkexsession* pSession,
	xbytesview* pSessionId
)
{
	if ( !xsshKexSessionValid(pSession) || !pSession->HasSessionId ||
		!xrtMemRangeValid(pSessionId, sizeof(*pSessionId)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pSessionId,
			sizeof(*pSessionId)
		) ) {
		return XSSH_ERROR_STATE;
	}
	pSessionId->Data = pSession->SessionId;
	pSessionId->Size = sizeof(pSession->SessionId);
	return XSSH_OK;
}



/* 返回客户端待确认的主机公钥。 */
xsshcode xrtSshKexSessionHostKey(
	const xsshkexsession* pSession,
	xbytesview* pHostKey
)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		!pSession->HostKeyVerified ||
		!xrtMemRangeValid(pHostKey, sizeof(*pHostKey)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKey,
			sizeof(*pHostKey)
		) ) {
		return XSSH_ERROR_STATE;
	}
	*pHostKey = pSession->ServerHostKey;
	return XSSH_OK;
}



/* 提交客户端主机信任决策。 */
xsshcode xrtSshKexSessionHostKeyAccept(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ||
		!pSession->MethodReadCommitted || !pSession->HostKeyVerified ) {
		return XSSH_ERROR_STATE;
	}
	pSession->HostKeyAccepted = true;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 显式终止 KEX 会话。 */
void xrtSshKexSessionFail(xsshkexsession* pSession)
{
	if ( xsshKexSessionValid(pSession) ) {
		xsshKexSessionSetFailed(pSession);
	}
}



/* 准备客户端 Curve25519 方法初始消息。 */
xsshcode xrtSshKexSessionEcdhInitPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->Role != XSSH_ROLE_CLIENT) ||
		pSession->MethodWriteCommitted ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshEcdhInitWrite(
		pWriter,
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) }
	);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_ECDH_INIT;
	}
	return Code;
}



/* 验证外部签名并准备服务端 Curve25519 方法回复。 */
xsshcode xrtSshKexSessionEcdhReplyPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter,
	xbytesview Signature
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->Role != XSSH_ROLE_SERVER) ||
		!pSession->MethodReadCommitted || !pSession->KeysDerived ||
		pSession->MethodWriteCommitted ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshEd25519HostKeyVerify(
		pSession->ServerHostKey,
		Signature,
		(xbytesview){
			pSession->ExchangeHash,
			sizeof(pSession->ExchangeHash)
		}
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshEcdhReplyWrite(
		pWriter,
		pSession->ServerHostKey,
		(xbytesview){ pSession->PublicKey, sizeof(pSession->PublicKey) },
		Signature
	);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_ECDH_REPLY;
	}
	return Code;
}



/* 准备无字段 NEWKEYS。 */
xsshcode xrtSshKexSessionNewKeysPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
)
{
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		!pSession->KeysDerived || !pSession->MethodWriteCommitted ||
		!pSession->MethodReadCommitted || pSession->LocalNewKeys ||
		(pSession->WritePending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pWriter,
			sizeof(*pWriter)
		) ||
		((pSession->Role == XSSH_ROLE_CLIENT) &&
		 !pSession->HostKeyAccepted) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshNewKeysWrite(pWriter);
	if ( Code == XSSH_OK ) {
		pSession->WritePending = XSSH_KEX_PACKET_NEWKEYS;
	}
	return Code;
}



/* 在 transport 提交后推进对应写方法。 */
xsshcode xrtSshKexSessionWriteCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshkexsessionpacket Packet;
	uint8 iMessage;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Write.Active ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		(pSession->WritePending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->WritePending;
	if ( Packet == XSSH_KEX_PACKET_NEWKEYS ) {
		if ( !xrtSshTransportCoreWriteKeysPending(pCore) ) {
			return XSSH_ERROR_STATE;
		}
		pSession->LocalNewKeys = true;
	} else {
		iMessage = Packet == XSSH_KEX_PACKET_ECDH_INIT ?
			XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY;
		if ( pCore->State.LocalKexRemaining[
			(size_t)iMessage - XSSH_KEX_METHOD_MIN
		] != 0u ) {
			return XSSH_ERROR_STATE;
		}
		pSession->MethodWriteCommitted = true;
	}
	pSession->WritePending = XSSH_KEX_PACKET_NONE;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 放弃尚未可靠发送的 KEX payload。 */
xsshcode xrtSshKexSessionWriteAbort(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->WritePending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	pSession->WritePending = XSSH_KEX_PACKET_NONE;
	return XSSH_OK;
}



/* 服务端准备客户端 ECDH_INIT，并在临时候选状态中派生密钥。 */
static xsshcode xsshKexSessionReadInitPrepare(
	xsshkexsession* pSession,
	xbytesview Payload
)
{
	xsshkexsession Session = *pSession;
	xsshecdhinit Message;
	xsshcode Code;

	memset(&Message, 0, sizeof(Message));
	Code = xrtSshEcdhInitRead(Payload, &Message);
	if ( Code == XSSH_OK &&
		(Message.ClientPublic.Size != XSSH_CURVE25519_PUBLIC_SIZE) ) {
		Code = XSSH_ERROR_PROTOCOL;
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			Session.PeerPublicKey,
			Message.ClientPublic.Data,
			sizeof(Session.PeerPublicKey)
		);
		Code = xrtSshCurve25519Shared(
			Session.PrivateKey,
			Session.PeerPublicKey,
			Session.SharedSecret
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshKexSessionDerive(&Session);
	}
	if ( Code == XSSH_OK ) {
		Session.ReadPending = XSSH_KEX_PACKET_ECDH_INIT;
		*pSession = Session;
	}
	xrtSecureZero(&Session, sizeof(Session));
	return Code;
}



/* 客户端准备服务端 ECDH_REPLY、验签并复制待信任主机公钥。 */
static xsshcode xsshKexSessionReadReplyPrepare(
	xsshkexsession* pSession,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
)
{
	xsshkexsession Session = *pSession;
	xsshecdhreply Message;
	xsshcode Code;

	memset(&Message, 0, sizeof(Message));
	Code = xrtSshEcdhReplyRead(Payload, &Message);
	if ( Code == XSSH_OK &&
		(Message.ServerPublic.Size != XSSH_CURVE25519_PUBLIC_SIZE) ) {
		Code = XSSH_ERROR_PROTOCOL;
	}
	if ( (Code == XSSH_OK) && (pHostKeySize != NULL) ) {
		*pHostKeySize = Message.ServerHostKey.Size;
	}
	if ( Code == XSSH_OK &&
		((iHostKeyCapacity < Message.ServerHostKey.Size) ||
		 !xrtMemRangeValid(pHostKeyStorage, Message.ServerHostKey.Size)) ) {
		Code = XSSH_ERROR_SPACE;
	}
	if ( Code == XSSH_OK &&
		(xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeyStorage,
			Message.ServerHostKey.Size
		) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pHostKeyStorage,
			Message.ServerHostKey.Size
		)) ) {
		Code = XSSH_ERROR_ARGUMENT;
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			Session.PeerPublicKey,
			Message.ServerPublic.Data,
			sizeof(Session.PeerPublicKey)
		);
		Session.ServerHostKey = Message.ServerHostKey;
		Code = xrtSshCurve25519Shared(
			Session.PrivateKey,
			Session.PeerPublicKey,
			Session.SharedSecret
		);
	}
	if ( Code == XSSH_OK ) {
		Code = xsshKexSessionDerive(&Session);
	}
	if ( Code == XSSH_OK ) {
		Code = xrtSshEd25519HostKeyVerify(
			Message.ServerHostKey,
			Message.Signature,
			(xbytesview){
				Session.ExchangeHash,
				sizeof(Session.ExchangeHash)
			}
		);
	}
	if ( Code == XSSH_OK ) {
		memcpy(
			pHostKeyStorage,
			Message.ServerHostKey.Data,
			Message.ServerHostKey.Size
		);
		Session.ServerHostKey = (xbytesview){
			(const unsigned char*)pHostKeyStorage,
			Message.ServerHostKey.Size
		};
		Session.ReadPending = XSSH_KEX_PACKET_ECDH_REPLY;
		*pSession = Session;
	}
	xrtSecureZero(&Session, sizeof(Session));
	return Code;
}



/* 准备一个 core 已认证的 KEX 方法或 NEWKEYS payload。 */
xsshcode xrtSshKexSessionReadPrepare(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) || !pSession->Active ||
		(pSession->ReadPending != XSSH_KEX_PACKET_NONE) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size == 0u) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) || xrtMemRangesOverlap(
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
		return XSSH_ERROR_STATE;
	}
	if ( (pHostKeySize != NULL) &&
		(!xrtMemRangeValid(pHostKeySize, sizeof(*pHostKeySize)) ||
		 xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pHostKeySize,
			sizeof(*pHostKeySize)
		 ) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pHostKeySize,
			sizeof(*pHostKeySize)
		 ) || xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pHostKeySize,
			sizeof(*pHostKeySize)
		 )) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xrtMemRangesOverlap(
		pSession,
		sizeof(*pSession),
		pHostKeyStorage,
		iHostKeyCapacity
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pHostKeyStorage,
		iHostKeyCapacity
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pHostKeySize != NULL ) {
		*pHostKeySize = 0u;
	}
	Code = xrtSshMessageType(Payload, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xsshKexSessionCoreReadMatches(pCore, Payload, iMessage) ) {
		return XSSH_ERROR_STATE;
	}
	if ( xsshKexSessionDiscardGuess(pCore, iMessage) ) {
		pSession->ReadPending = XSSH_KEX_PACKET_DISCARD;
		return XSSH_OK;
	}
	if ( iMessage == XSSH_MSG_NEWKEYS ) {
		if ( pSession->PeerNewKeys ||
			(pSession->Role == XSSH_ROLE_CLIENT ?
			 !pSession->MethodReadCommitted :
			 !pSession->MethodWriteCommitted) ) {
			return XSSH_ERROR_STATE;
		}
		Code = xrtSshNewKeysRead(Payload);
		if ( Code == XSSH_OK ) {
			pSession->ReadPending = XSSH_KEX_PACKET_NEWKEYS;
		}
		return Code;
	}
	if ( pSession->Role == XSSH_ROLE_SERVER ) {
		if ( (iMessage != XSSH_MSG_KEX_ECDH_INIT) ||
			pSession->MethodReadCommitted ) {
			return XSSH_ERROR_PROTOCOL;
		}
		return xsshKexSessionReadInitPrepare(pSession, Payload);
	}
	if ( (iMessage != XSSH_MSG_KEX_ECDH_REPLY) ||
		!pSession->MethodWriteCommitted ||
		pSession->MethodReadCommitted ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xsshKexSessionReadReplyPrepare(
		pSession,
		Payload,
		pHostKeyStorage,
		iHostKeyCapacity,
		pHostKeySize
	);
}



/* 在 transport 已提交输入后推进 KEX 读事务。 */
xsshcode xrtSshKexSessionReadCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	xsshkexsessionpacket Packet;
	uint8 iMessage;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || pCore->Read.Active ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		(pSession->ReadPending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	Packet = pSession->ReadPending;
	if ( Packet == XSSH_KEX_PACKET_DISCARD ) {
		if ( !pCore->State.PeerGuessSeen ) {
			return XSSH_ERROR_STATE;
		}
	} else if ( Packet == XSSH_KEX_PACKET_NEWKEYS ) {
		if ( !xrtSshTransportCoreReadKeysPending(pCore) ) {
			return XSSH_ERROR_STATE;
		}
		pSession->PeerNewKeys = true;
	} else {
		iMessage = Packet == XSSH_KEX_PACKET_ECDH_INIT ?
			XSSH_MSG_KEX_ECDH_INIT : XSSH_MSG_KEX_ECDH_REPLY;
		if ( pCore->State.PeerKexRemaining[
			(size_t)iMessage - XSSH_KEX_METHOD_MIN
		] != 0u ) {
			return XSSH_ERROR_STATE;
		}
		pSession->MethodReadCommitted = true;
		if ( Packet == XSSH_KEX_PACKET_ECDH_REPLY ) {
			pSession->HostKeyVerified = true;
		}
	}
	pSession->ReadPending = XSSH_KEX_PACKET_NONE;
	xsshKexSessionUpdatePhase(pSession);
	return XSSH_OK;
}



/* 已认证输入不可回滚，放弃时终止会话。 */
xsshcode xrtSshKexSessionReadAbort(xsshkexsession* pSession)
{
	if ( !xsshKexSessionValid(pSession) ||
		(pSession->ReadPending == XSSH_KEX_PACKET_NONE) ) {
		return XSSH_ERROR_STATE;
	}
	xsshKexSessionSetFailed(pSession);
	return XSSH_OK;
}



/* 激活角色对应的本端写方向密钥。 */
xsshcode xrtSshKexSessionActivateWrite(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xbytesview Key;
	xbytesview IV;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || !pSession->KeysDerived ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		!pSession->LocalNewKeys || pSession->WriteActivated ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		Key = (xbytesview){
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize
		};
		IV = (xbytesview){
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		};
	} else {
		Key = (xbytesview){
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize
		};
		IV = (xbytesview){
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		};
	}
	Code = xrtSshTransportCoreSetWriteAesGcm(pCore, Key, IV, iNowMs);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSecureZero((void*)Key.Data, Key.Size);
	xrtSecureZero((void*)IV.Data, IV.Size);
	pSession->WriteActivated = true;
	xsshKexSessionUpdatePhase(pSession);
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		xrtSecureZero(pSession->PrivateKey, sizeof(pSession->PrivateKey));
		xrtSecureZero(pSession->SharedSecret, sizeof(pSession->SharedSecret));
	}
	return XSSH_OK;
}



/* 激活角色对应的 peer 读方向密钥。 */
xsshcode xrtSshKexSessionActivateRead(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
)
{
	xbytesview Key;
	xbytesview IV;
	xsshcode Code;

	if ( !xsshKexSessionValid(pSession) ||
		!xrtMemRangeValid(pCore, sizeof(*pCore)) ||
		(pCore->State.Role != pSession->Role) || !pSession->KeysDerived ||
		xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) ||
		!pSession->PeerNewKeys || pSession->ReadActivated ) {
		return XSSH_ERROR_STATE;
	}
	if ( pSession->Role == XSSH_ROLE_CLIENT ) {
		Key = (xbytesview){
			pSession->ServerToClientKey,
			pSession->ServerToClientKeySize
		};
		IV = (xbytesview){
			pSession->ServerToClientIV,
			sizeof(pSession->ServerToClientIV)
		};
	} else {
		Key = (xbytesview){
			pSession->ClientToServerKey,
			pSession->ClientToServerKeySize
		};
		IV = (xbytesview){
			pSession->ClientToServerIV,
			sizeof(pSession->ClientToServerIV)
		};
	}
	Code = xrtSshTransportCoreSetReadAesGcm(pCore, Key, IV, iNowMs);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	xrtSecureZero((void*)Key.Data, Key.Size);
	xrtSecureZero((void*)IV.Data, IV.Size);
	pSession->ReadActivated = true;
	xsshKexSessionUpdatePhase(pSession);
	if ( pSession->Phase == XSSH_KEX_SESSION_COMPLETE ) {
		xrtSecureZero(pSession->PrivateKey, sizeof(pSession->PrivateKey));
		xrtSecureZero(pSession->SharedSecret, sizeof(pSession->SharedSecret));
	}
	return XSSH_OK;
}



/* 校验会话与 transport core 双方都已完成一代 KEX。 */
bool xrtSshKexSessionComplete(
	const xsshkexsession* pSession,
	const xsshtransportcore* pCore
)
{
	return xsshKexSessionValid(pSession) &&
		xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		!xrtMemRangesOverlap(
			pSession,
			sizeof(*pSession),
			pCore,
			sizeof(*pCore)
		) &&
		(pCore->State.Role == pSession->Role) &&
		(pSession->Phase == XSSH_KEX_SESSION_COMPLETE) &&
		pSession->WriteActivated && pSession->ReadActivated &&
		xrtSshTransportCoreKexComplete(pCore);
}

#endif
