#include <string.h>

#include <xrt/ssh_transport_rekey.h>



#if defined(XSSH_FEATURE_TRANSPORT_REKEY)

/* 验证软包阈值不会超过不可取消的协议硬上限。 */
static bool xsshRekeyPolicyValid(const xsshrekeypolicy* pPolicy)
{
	if ( (pPolicy == NULL) || (pPolicy->HardPacketLimit == 0u) ||
		(pPolicy->HardPacketLimit > XSSH_REKEY_HARD_PACKET_LIMIT) ) {
		return false;
	}
	return ((pPolicy->SendPacketLimit == 0u) ||
		(pPolicy->SendPacketLimit <= pPolicy->HardPacketLimit)) &&
		((pPolicy->ReceivePacketLimit == 0u) ||
		(pPolicy->ReceivePacketLimit <= pPolicy->HardPacketLimit));
}



/* 计数使用饱和加法，异常大的调用值只会提前触发 rekey。 */
static uint64 xsshRekeyAdd(uint64 iLeft, uint64 iRight)
{
	return iRight > (UINT64_MAX - iLeft) ?
		UINT64_MAX : iLeft + iRight;
}



/* 比较启用的单项软阈值。 */
static bool xsshRekeyLimit(uint64 iValue, uint64 iLimit)
{
	return (iLimit != 0u) && (iValue >= iLimit);
}



/* 合并手动、时间和双向计数产生的当前决策。 */
static xsshrekeydecision xsshRekeyCurrent(
	const xsshrekeystate* pState,
	uint64 iNowMs
)
{
	uint64 iSendElapsed = iNowMs >= pState->SendStartedMs ?
		iNowMs - pState->SendStartedMs : 0u;
	uint64 iReceiveElapsed = iNowMs >= pState->ReceiveStartedMs ?
		iNowMs - pState->ReceiveStartedMs : 0u;

	if ( (pState->Sent.Packets >= pState->Policy.HardPacketLimit) ||
		(pState->Received.Packets >= pState->Policy.HardPacketLimit) ) {
		return XSSH_REKEY_REQUIRED;
	}
	if ( pState->Requested ||
		xsshRekeyLimit(iSendElapsed, pState->Policy.TimeLimitMs) ||
		xsshRekeyLimit(iReceiveElapsed, pState->Policy.TimeLimitMs) ||
		xsshRekeyLimit(pState->Sent.Bytes, pState->Policy.ByteLimit) ||
		xsshRekeyLimit(pState->Received.Bytes, pState->Policy.ByteLimit) ||
		xsshRekeyLimit(pState->Sent.Blocks, pState->Policy.BlockLimit) ||
		xsshRekeyLimit(pState->Received.Blocks, pState->Policy.BlockLimit) ||
		xsshRekeyLimit(
			pState->Sent.Packets,
			pState->Policy.SendPacketLimit
		) || xsshRekeyLimit(
			pState->Received.Packets,
			pState->Policy.ReceivePacketLimit
		) ) {
		return XSSH_REKEY_RECOMMENDED;
	}
	return XSSH_REKEY_NONE;
}



/* 原子预留一个方向的下一包，并返回更新后的策略决策。 */
static xsshcode xsshRekeyReserve(
	xsshrekeystate* pState,
	xsshrekeycounter* pCounter,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	xsshrekeycounter Counter;
	xsshrekeydecision Decision;

	if ( (pState == NULL) || (pCounter == NULL) || (pDecision == NULL) ||
		!xsshRekeyPolicyValid(&pState->Policy) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( xsshRekeyCurrent(pState, iNowMs) == XSSH_REKEY_REQUIRED ) {
		*pDecision = XSSH_REKEY_REQUIRED;
		return XSSH_OK;
	}
	Counter = *pCounter;
	Counter.Packets += 1u;
	Counter.Bytes = xsshRekeyAdd(Counter.Bytes, iWireBytes);
	Counter.Blocks = xsshRekeyAdd(Counter.Blocks, iCipherBlocks);
	*pCounter = Counter;
	Decision = xsshRekeyCurrent(pState, iNowMs);
	if ( Decision == XSSH_REKEY_REQUIRED ) {
		Decision = XSSH_REKEY_RECOMMENDED;
	}
	*pDecision = Decision;
	return XSSH_OK;
}



/* 建立兼顾 RFC 推荐值和状态机提前量的默认策略。 */
void xrtSshRekeyPolicyInit(xsshrekeypolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		return;
	}
	pPolicy->ByteLimit = XSSH_REKEY_DEFAULT_BYTE_LIMIT;
	pPolicy->SendPacketLimit = XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT;
	pPolicy->ReceivePacketLimit = XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT;
	pPolicy->BlockLimit = XSSH_REKEY_DEFAULT_BLOCK_LIMIT;
	pPolicy->TimeLimitMs = XSSH_REKEY_DEFAULT_TIME_LIMIT_MS;
	pPolicy->HardPacketLimit = XSSH_REKEY_HARD_PACKET_LIMIT;
}



/* 复制显式策略或默认策略，并开始第一代密钥计数。 */
bool xrtSshRekeyInit(
	xsshrekeystate* pState,
	const xsshrekeypolicy* pPolicy,
	uint64 iNowMs
)
{
	xsshrekeypolicy Policy;
	xsshrekeystate State;

	if ( pState == NULL ) {
		return false;
	}
	if ( pPolicy == NULL ) {
		xrtSshRekeyPolicyInit(&Policy);
		pPolicy = &Policy;
	}
	if ( !xsshRekeyPolicyValid(pPolicy) ) {
		return false;
	}
	memset(&State, 0, sizeof(State));
	State.Policy = *pPolicy;
	State.SendStartedMs = iNowMs;
	State.ReceiveStartedMs = iNowMs;
	*pState = State;
	return true;
}



/* 保留策略并重置新一代密钥的全部运行计数。 */
bool xrtSshRekeyReset(xsshrekeystate* pState, uint64 iNowMs)
{
	xsshrekeypolicy Policy;

	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	Policy = pState->Policy;
	memset(pState, 0, sizeof(*pState));
	pState->Policy = Policy;
	pState->SendStartedMs = iNowMs;
	pState->ReceiveStartedMs = iNowMs;
	return true;
}



/* 写密钥提交只开始新的发送代，不影响已先行收到的新密钥数据。 */
bool xrtSshRekeyResetSend(xsshrekeystate* pState, uint64 iNowMs)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	memset(&pState->Sent, 0, sizeof(pState->Sent));
	pState->SendStartedMs = iNowMs;
	return true;
}



/* 读密钥提交只开始新的接收代，不影响已先行发送的新密钥数据。 */
bool xrtSshRekeyResetReceive(xsshrekeystate* pState, uint64 iNowMs)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	memset(&pState->Received, 0, sizeof(pState->Received));
	pState->ReceiveStartedMs = iNowMs;
	return true;
}



/* KEX 完成只清除请求标志，不能抹掉已经使用的新代额度。 */
bool xrtSshRekeyComplete(xsshrekeystate* pState)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	pState->Requested = false;
	return true;
}



/* 记录应用、算法或对端触发的主动 rekey 请求。 */
bool xrtSshRekeyRequest(xsshrekeystate* pState)
{
	if ( (pState == NULL) || !xsshRekeyPolicyValid(&pState->Policy) ) {
		return false;
	}
	pState->Requested = true;
	return true;
}



/* 读取当前决策，不改变计数状态。 */
xsshcode xrtSshRekeyCheck(
	const xsshrekeystate* pState,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	if ( (pState == NULL) || (pDecision == NULL) ||
		!xsshRekeyPolicyValid(&pState->Policy) ||
		xrtMemRangesOverlap(
			pState,
			sizeof(*pState),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	*pDecision = xsshRekeyCurrent(pState, iNowMs);
	return XSSH_OK;
}



/* 预留发送方向下一包。 */
xsshcode xrtSshRekeyReserveSend(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	if ( pState == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRekeyReserve(
		pState,
		&pState->Sent,
		iWireBytes,
		iCipherBlocks,
		iNowMs,
		pDecision
	);
}



/* 预留接收方向下一包。 */
xsshcode xrtSshRekeyReserveReceive(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
)
{
	if ( pState == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return xsshRekeyReserve(
		pState,
		&pState->Received,
		iWireBytes,
		iCipherBlocks,
		iNowMs,
		pDecision
	);
}

#endif
