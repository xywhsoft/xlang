#include <string.h>

#include <xrt/ssh_transport_core.h>



#if defined(XSSH_FEATURE_TRANSPORT_CORE)

#define XSSH_TRANSPORT_CORE_GUARD UINT32_C(0x53544352)



/* 验证 core 自身和唯一写事务的一致性。 */
static bool xsshTransportCoreValid(const xsshtransportcore* pCore)
{
	return xrtMemRangeValid(pCore, sizeof(*pCore)) &&
		(pCore->Guard == XSSH_TRANSPORT_CORE_GUARD) &&
		(pCore->Write.Active == pCore->Codec.WritePending);
}



/* 清除不拥有外部资源的待提交描述。 */
static void xsshTransportPendingClear(xsshtransportpending* pPending)
{
	memset(pPending, 0, sizeof(*pPending));
}



/* 从完整 payload 分类状态机需要特殊提交的消息。 */
static xsshcode xsshTransportPendingRead(
	xbytesview Payload,
	xsshtransportpending* pPending
)
{
	xsshtransportpending Pending;
	xsshkexinit KexInit;
	xsshcode Code;

	memset(&Pending, 0, sizeof(Pending));
	Code = xrtSshMessageType(Payload, &Pending.Message);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Pending.Message == XSSH_MSG_KEXINIT ) {
		Code = xrtSshKexInitRead(Payload, &KexInit);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_KEXINIT;
		Pending.FirstKexPacketFollows = KexInit.FirstKexPacketFollows;
	} else if ( Pending.Message == XSSH_MSG_NEWKEYS ) {
		Code = xrtSshNewKeysRead(Payload);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_NEWKEYS;
	} else if ( Pending.Message == XSSH_MSG_USERAUTH_SUCCESS ) {
		if ( Payload.Size != 1u ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Pending.Kind = XSSH_TRANSPORT_PACKET_AUTH_SUCCESS;
	} else {
		Pending.Kind = XSSH_TRANSPORT_PACKET_MESSAGE;
	}
	Pending.Active = true;
	*pPending = Pending;
	return XSSH_OK;
}



/* 按消息类别执行不改变状态的方向检查。 */
static xsshcode xsshTransportCoreStateCheck(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	const xsshtransportpending* pPending
)
{
	switch ( pPending->Kind ) {
		case XSSH_TRANSPORT_PACKET_KEXINIT:
			return xrtSshTransportKexInitCheck(&pCore->State, Direction);
		case XSSH_TRANSPORT_PACKET_NEWKEYS:
			return xrtSshTransportNewKeysCheck(&pCore->State, Direction);
		case XSSH_TRANSPORT_PACKET_AUTH_SUCCESS:
			return xrtSshTransportAuthSuccessCheck(
				&pCore->State,
				Direction
			);
		case XSSH_TRANSPORT_PACKET_MESSAGE:
			return xrtSshTransportMessageCheck(
				&pCore->State,
				Direction,
				pPending->Message
			);
		default:
			return XSSH_ERROR_STATE;
	}
}



/* 在可靠边界按消息类别推进状态，并发布 NEWKEYS 动作。 */
static xsshcode xsshTransportCoreStateCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	const xsshtransportpending* pPending,
	uint32* pActions
)
{
	*pActions = XSSH_TRANSPORT_ACTION_NONE;
	switch ( pPending->Kind ) {
		case XSSH_TRANSPORT_PACKET_KEXINIT:
			return xrtSshTransportKexInitCommit(
				&pCore->State,
				Direction,
				pPending->FirstKexPacketFollows
			);
		case XSSH_TRANSPORT_PACKET_NEWKEYS:
			return xrtSshTransportNewKeysCommit(
				&pCore->State,
				Direction,
				pActions
			);
		case XSSH_TRANSPORT_PACKET_AUTH_SUCCESS:
			return xrtSshTransportAuthSuccessCommit(
				&pCore->State,
				Direction
			);
		case XSSH_TRANSPORT_PACKET_MESSAGE:
			return xrtSshTransportMessageCommit(
				&pCore->State,
				Direction,
				pPending->Message
			);
		default:
			return XSSH_ERROR_STATE;
	}
}



/* AES-GCM 只统计加密 packet body 的十六字节块。 */
static uint64 xsshTransportCoreCipherBlocks(
	xsshpacketmode Mode,
	uint32 iPacketSize
)
{
	return Mode == XSSH_PACKET_MODE_AES_GCM ?
		(uint64)(iPacketSize / XSSH_AES_GCM_BLOCK_SIZE) : 0u;
}



/* 不可恢复错误关闭状态机并解除本地写准备，避免继续复用状态。 */
static void xsshTransportCoreFail(xsshtransportcore* pCore)
{
	if ( pCore->Codec.WritePending ) {
		(void)xrtSshPacketCodecWriteAbort(&pCore->Codec);
	}
	xsshTransportPendingClear(&pCore->Write);
	xsshTransportPendingClear(&pCore->Read);
	pCore->WriteKeyActions = 0u;
	pCore->ReadKeyActions = 0u;
	pCore->KexCompletePending = false;
	xrtSshTransportClose(&pCore->State);
}



/* 记录方向性 NEWKEYS 动作，实际 cipher 激活前保持该方向关闭。 */
static void xsshTransportCoreKeyActions(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	uint32 iActions
)
{
	if ( (iActions & XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS) != 0u ) {
		if ( Direction == XSSH_TRANSPORT_LOCAL ) {
			pCore->WriteKeyActions = iActions;
		} else {
			pCore->ReadKeyActions = iActions;
		}
	}
	if ( (iActions & XSSH_TRANSPORT_ACTION_KEX_COMPLETE) != 0u ) {
		pCore->KexCompletePending = true;
	}
}



/* 只有双向新 cipher 都已提交后才结束本轮 rekey 请求。 */
static bool xsshTransportCoreCompleteReady(xsshtransportcore* pCore)
{
	if ( !pCore->KexCompletePending ||
		(pCore->WriteKeyActions != 0u) ||
		(pCore->ReadKeyActions != 0u) ) {
		return true;
	}
	if ( !xrtSshRekeyComplete(&pCore->Rekey) ) {
		return false;
	}
	pCore->KexCompletePending = false;
	return true;
}



/* 组合三个纯状态层，初始化失败不会发布半成品。 */
bool xrtSshTransportCoreInit(
	xsshtransportcore* pCore,
	xsshrole Role,
	uint32 iMaxPacketSize,
	const xsshrekeypolicy* pRekeyPolicy,
	uint64 iNowMs
)
{
	xsshtransportcore Core;

	if ( !xrtMemRangeValid(pCore, sizeof(*pCore)) ) {
		return false;
	}
	memset(&Core, 0, sizeof(Core));
	if ( (xrtSshPacketCodecInit(
		&Core.Codec,
		iMaxPacketSize
	) != XSSH_OK) || !xrtSshTransportStateInit(&Core.State, Role) ||
		!xrtSshRekeyInit(&Core.Rekey, pRekeyPolicy, iNowMs) ) {
		xrtSshPacketCodecClear(&Core.Codec);
		xrtSecureZero(&Core, sizeof(Core));
		return false;
	}
	Core.Guard = XSSH_TRANSPORT_CORE_GUARD;
	xrtSecureZero(pCore, sizeof(*pCore));
	*pCore = Core;
	xrtSecureZero(&Core, sizeof(Core));
	return true;
}



/* Core 唯一秘密是展开后的 active cipher，统一安全清零。 */
void xrtSshTransportCoreClear(xsshtransportcore* pCore)
{
	if ( pCore == NULL ) {
		return;
	}
	xrtSecureZero(pCore, sizeof(*pCore));
}



/* Identification 不经过 packet codec，但仍属于同一协议状态。 */
xsshcode xrtSshTransportCoreIdentificationCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportIdentificationCommit(&pCore->State, Direction);
}



/* 状态机开放后仍必须等待对应方向的新 cipher 实际提交。 */
bool xrtSshTransportCoreCanApplication(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
)
{
	if ( !xsshTransportCoreValid(pCore) ) {
		return false;
	}
	if ( ((Direction == XSSH_TRANSPORT_LOCAL) &&
		 (pCore->WriteKeyActions != 0u)) ||
		((Direction == XSSH_TRANSPORT_PEER) &&
		 (pCore->ReadKeyActions != 0u)) ) {
		return false;
	}
	return xrtSshTransportCanApplication(&pCore->State, Direction);
}



/* KEX 回复判断直接复用顺序状态。 */
bool xrtSshTransportCoreKexReplyNeeded(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		xrtSshTransportKexReplyNeeded(&pCore->State);
}



/* KEX 配置期间不得存在尚未完成的 packet 或 NEWKEYS 密钥动作。 */
xsshcode xrtSshTransportCoreKexConfigure(
	xsshtransportcore* pCore,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
)
{
	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		pCore->Read.Active || (pCore->WriteKeyActions != 0u) ||
		(pCore->ReadKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshTransportKexConfigure(
		&pCore->State,
		pLocal,
		pPeer,
		pNegotiation,
		pRules
	);
}



/* 主动请求只改变预算策略状态，不隐式生成 KEXINIT。 */
bool xrtSshTransportCoreRekeyRequest(xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		xrtSshRekeyRequest(&pCore->Rekey);
}



/* 查询操作保持 core 不变。 */
xsshcode xrtSshTransportCoreRekeyCheck(
	const xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	if ( !xsshTransportCoreValid(pCore) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pDecision,
		sizeof(*pDecision)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshRekeyCheck(&pCore->Rekey, iNowMs, pDecision);
}



/* NEWKEYS 未应用时不能用旧方向模式探测下一包。 */
xsshcode xrtSshTransportCoreInspect(
	const xsshtransportcore* pCore,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
)
{
	if ( !xsshTransportCoreValid(pCore) ||
		(pCore->ReadKeyActions != 0u) || pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pNeed, sizeof(*pNeed)) ||
		xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pReader,
		sizeof(*pReader)
	) || xrtMemRangesOverlap(
		pCore,
		sizeof(*pCore),
		pNeed,
		sizeof(*pNeed)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xrtSshPacketCodecInspect(&pCore->Codec, pReader, pNeed);
}



/* 状态检查和硬额度检查都发生在生成线路字节之前。 */
xsshcode xrtSshTransportCoreWritePrepareWithPadding(
	xsshtransportcore* pCore,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	uint64 iNowMs
)
{
	xsshtransportpending Pending;
	xsshrekeydecision Decision;
	size_t iStart;
	size_t iWireSize;
	uint32 iPacketSize;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		(pCore->WriteKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pWriter->Data, pWriter->Capacity) ||
		(pWriter->Size > pWriter->Capacity) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pWriter,
			sizeof(*pWriter)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pWriter->Data,
			pWriter->Capacity
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			Payload.Data,
			Payload.Size
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportPendingRead(Payload, &Pending);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&Pending
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		return XSSH_ERROR_STATE;
	}
	iStart = pWriter->Size;
	Code = xrtSshPacketCodecWritePrepareWithPadding(
		&pCore->Codec,
		pWriter,
		Payload,
		pPadding,
		pUserData
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iWireSize = pWriter->Size - iStart;
	iPacketSize = (uint32)(iWireSize - 4u -
		(pCore->Codec.WriteMode == XSSH_PACKET_MODE_AES_GCM ?
		 XSSH_AES_GCM_TAG_SIZE : 0u));
	Pending.WireBytes = iWireSize;
	Pending.CipherBlocks = xsshTransportCoreCipherBlocks(
		pCore->Codec.WriteMode,
		iPacketSize
	);
	pCore->Write = Pending;
	return XSSH_OK;
}



/* 可靠入队后按 codec、协议和预算三个边界一次提交。 */
xsshcode xrtSshTransportCoreWriteCommit(
	xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Write.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&pCore->Write
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshRekeyReserveSend(
			&pCore->Rekey,
			pCore->Write.WireBytes,
			pCore->Write.CipherBlocks,
			iNowMs,
			&Decision
		);
	}
	if ( (Code != XSSH_OK) || (Decision == XSSH_REKEY_REQUIRED) ) {
		xsshTransportCoreFail(pCore);
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshTransportCoreStateCommit(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		&pCore->Write,
		&iActions
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshPacketCodecWriteCommit(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Write);
	xsshTransportCoreKeyActions(
		pCore,
		XSSH_TRANSPORT_LOCAL,
		iActions
	);
	*pDecision = Decision;
	return XSSH_OK;
}



/* 未进入网络队列的 packet 可以无损取消。 */
xsshcode xrtSshTransportCoreWriteAbort(xsshtransportcore* pCore)
{
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Write.Active ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshPacketCodecWriteAbort(&pCore->Codec);
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Write);
	return XSSH_OK;
}



/* 完整包先认证和分类，状态检查成功后才向调用方发布 view。 */
xsshcode xrtSshTransportCoreReadPrepare(
	xsshtransportcore* pCore,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
)
{
	xsshtransportpending Pending;
	xsshrekeydecision Decision;
	xsshpacketneed Need;
	xsshpacketview Packet;
	xsshreader Reader;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Read.Active ||
		(pCore->ReadKeyActions != 0u) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pReader, sizeof(*pReader)) ||
		!xrtMemRangeValid(pPacket, sizeof(*pPacket)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pReader,
			sizeof(*pReader)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPacket,
			sizeof(*pPacket)
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pReader->Source.Data,
			pReader->Source.Size
		) || xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pPlain,
			iPlainCapacity
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshPacketCodecInspect(&pCore->Codec, pReader, &Need);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xsshTransportCoreFail(pCore);
		}
		return Code;
	}
	if ( xrtSshReaderRemaining(pReader) < Need.WireSize ) {
		return XSSH_NEED_MORE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		return XSSH_ERROR_STATE;
	}
	Reader = *pReader;
	Code = xrtSshPacketCodecRead(
		&pCore->Codec,
		&Reader,
		&Packet,
		pPlain,
		iPlainCapacity
	);
	if ( Code != XSSH_OK ) {
		if ( (Code == XSSH_ERROR_PROTOCOL) ||
			(Code == XSSH_ERROR_OVERFLOW) ||
			(Code == XSSH_ERROR_AUTHENTICATION) ||
			(Code == XSSH_ERROR_STATE) ) {
			xsshTransportCoreFail(pCore);
		}
		return Code;
	}
	Code = xsshTransportPendingRead(Packet.Payload, &Pending);
	if ( Code == XSSH_OK ) {
		Code = xsshTransportCoreStateCheck(
			pCore,
			XSSH_TRANSPORT_PEER,
			&Pending
		);
	}
	if ( Code != XSSH_OK ) {
		if ( Need.PlainSize != 0u ) {
			xrtSecureZero(pPlain, Need.PlainSize);
		}
		xsshTransportCoreFail(pCore);
		return Code;
	}
	Pending.WireBytes = Need.WireSize;
	Pending.CipherBlocks = xsshTransportCoreCipherBlocks(
		pCore->Codec.ReadMode,
		Need.PacketSize
	);
	pCore->Read = Pending;
	*pReader = Reader;
	*pPacket = Packet;
	return XSSH_OK;
}



/* 已认证包只在上层解析接受后登记预算并推进协议。 */
xsshcode xrtSshTransportCoreReadCommit(
	xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || !pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtMemRangeValid(pDecision, sizeof(*pDecision)) ||
		xrtMemRangesOverlap(
			pCore,
			sizeof(*pCore),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshTransportCoreStateCheck(
		pCore,
		XSSH_TRANSPORT_PEER,
		&pCore->Read
	);
	if ( Code == XSSH_OK ) {
		Code = xrtSshRekeyReserveReceive(
			&pCore->Rekey,
			pCore->Read.WireBytes,
			pCore->Read.CipherBlocks,
			iNowMs,
			&Decision
		);
	}
	if ( (Code != XSSH_OK) || (Decision == XSSH_REKEY_REQUIRED) ) {
		xsshTransportCoreFail(pCore);
		return Code == XSSH_OK ? XSSH_ERROR_STATE : Code;
	}
	Code = xsshTransportCoreStateCommit(
		pCore,
		XSSH_TRANSPORT_PEER,
		&pCore->Read,
		&iActions
	);
	if ( Code != XSSH_OK ) {
		xsshTransportCoreFail(pCore);
		return Code;
	}
	xsshTransportPendingClear(&pCore->Read);
	xsshTransportCoreKeyActions(
		pCore,
		XSSH_TRANSPORT_PEER,
		iActions
	);
	*pDecision = Decision;
	return XSSH_OK;
}



/* Codec 已消费并认证该包，放弃只能关闭而不能伪造回滚。 */
xsshcode xrtSshTransportCoreReadAbort(xsshtransportcore* pCore)
{
	if ( !xsshTransportCoreValid(pCore) || !pCore->Read.Active ) {
		return XSSH_ERROR_STATE;
	}
	xsshTransportCoreFail(pCore);
	return XSSH_OK;
}



/* 写方向动作非零表示 codec 仍停留在旧密钥。 */
bool xrtSshTransportCoreWriteKeysPending(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->WriteKeyActions != 0u);
}



/* 读方向动作非零表示 codec 仍停留在旧密钥。 */
bool xrtSshTransportCoreReadKeysPending(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->ReadKeyActions != 0u);
}



/* 新写 cipher、strict sequence 和发送代在同一调用中依次提交。 */
xsshcode xrtSshTransportCoreSetWriteAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Write.Active ||
		(pCore->WriteKeyActions == 0u) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iActions = pCore->WriteKeyActions;
	Code = xrtSshPacketCodecSetWriteAesGcm(
		&pCore->Codec,
		Key,
		InitialIV
	);
	if ( (Code == XSSH_OK) &&
		((iActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) ) {
		Code = xrtSshPacketCodecResetWriteSequence(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshRekeyResetSend(&pCore->Rekey, iNowMs) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	pCore->WriteKeyActions = 0u;
	if ( !xsshTransportCoreCompleteReady(pCore) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 新读 cipher、strict sequence 和接收代在同一调用中依次提交。 */
xsshcode xrtSshTransportCoreSetReadAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	uint64 iNowMs
)
{
	xsshrekeydecision Decision;
	uint32 iActions;
	xsshcode Code;

	if ( !xsshTransportCoreValid(pCore) || pCore->Read.Active ||
		(pCore->ReadKeyActions == 0u) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshRekeyCheck(&pCore->Rekey, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	iActions = pCore->ReadKeyActions;
	Code = xrtSshPacketCodecSetReadAesGcm(
		&pCore->Codec,
		Key,
		InitialIV
	);
	if ( (Code == XSSH_OK) &&
		((iActions & XSSH_TRANSPORT_ACTION_RESET_SEQUENCE) != 0u) ) {
		Code = xrtSshPacketCodecResetReadSequence(&pCore->Codec);
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshRekeyResetReceive(&pCore->Rekey, iNowMs) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	pCore->ReadKeyActions = 0u;
	if ( !xsshTransportCoreCompleteReady(pCore) ) {
		xsshTransportCoreFail(pCore);
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* OPEN 状态不能早于两个方向 cipher 的实际激活。 */
bool xrtSshTransportCoreKexComplete(const xsshtransportcore* pCore)
{
	return xsshTransportCoreValid(pCore) &&
		(pCore->State.KexCount != 0u) &&
		(pCore->State.Phase == XSSH_TRANSPORT_OPEN) &&
		(pCore->WriteKeyActions == 0u) &&
		(pCore->ReadKeyActions == 0u) &&
		!pCore->KexCompletePending;
}



/* Close 保留 cipher 供随后统一 Clear，但禁止任何继续推进。 */
void xrtSshTransportCoreClose(xsshtransportcore* pCore)
{
	if ( xsshTransportCoreValid(pCore) ) {
		xsshTransportCoreFail(pCore);
	}
}

#endif
