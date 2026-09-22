#include <string.h>

#include <xrt/ssh_auth_guard.h>



#if defined(XSSH_FEATURE_AUTH_GUARD)

/* 校验公开状态没有被未初始化或越界枚举伪造。 */
static bool xsshAuthGuardValid(const xsshauthguard* pGuard)
{
	return (pGuard != NULL) && pGuard->Initialized &&
		(pGuard->Exhaustion >= XSSH_AUTH_EXHAUST_NONE) &&
		(pGuard->Exhaustion <= XSSH_AUTH_EXHAUST_BYTES);
}



/* 无符号计数使用饱和加法，永不回绕。 */
static uint64 xsshAuthGuardAdd64(uint64 iLeft, uint64 iRight)
{
	return iRight > (UINT64_MAX - iLeft) ?
		UINT64_MAX : iLeft + iRight;
}



/* 32 位计数增加一项时饱和。 */
static uint32 xsshAuthGuardIncrement(uint32 iValue)
{
	return iValue == UINT32_MAX ? UINT32_MAX : iValue + 1u;
}



/* 判断启用的 64 位上限是否已经被超过。 */
static bool xsshAuthGuardLimit64(uint64 iValue, uint64 iLimit)
{
	return (iLimit != 0u) && (iValue > iLimit);
}



/* 判断启用的 32 位上限是否已经被超过。 */
static bool xsshAuthGuardLimit32(uint32 iValue, uint32 iLimit)
{
	return (iLimit != 0u) && (iValue > iLimit);
}



/* 按稳定优先级返回当前首个资源耗尽原因。 */
static xsshauthexhaustion xsshAuthGuardCurrent(
	const xsshauthguard* pGuard,
	uint64 iNowMs
)
{
	uint64 iElapsed = iNowMs >= pGuard->StartedMs ?
		iNowMs - pGuard->StartedMs : 0u;

	if ( pGuard->Exhaustion != XSSH_AUTH_EXHAUST_NONE ) {
		return pGuard->Exhaustion;
	}
	if ( (pGuard->Policy.TimeoutMs != 0u) &&
		(iElapsed >= pGuard->Policy.TimeoutMs) ) {
		return XSSH_AUTH_EXHAUST_TIMEOUT;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Attempts,
		pGuard->Policy.AttemptLimit
	) ) {
		return XSSH_AUTH_EXHAUST_ATTEMPTS;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Rounds,
		pGuard->Policy.RoundLimit
	) ) {
		return XSSH_AUTH_EXHAUST_ROUNDS;
	}
	if ( xsshAuthGuardLimit32(
		pGuard->Messages,
		pGuard->Policy.MessageLimit
	) ) {
		return XSSH_AUTH_EXHAUST_MESSAGES;
	}
	if ( xsshAuthGuardLimit64(
		pGuard->Bytes,
		pGuard->Policy.ByteLimit
	) ) {
		return XSSH_AUTH_EXHAUST_BYTES;
	}
	return XSSH_AUTH_EXHAUST_NONE;
}



/* 初始化 RFC 推荐超时、尝试数和保守资源预算。 */
void xrtSshAuthGuardPolicyInit(xsshauthguardpolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		return;
	}
	pPolicy->TimeoutMs = XSSH_AUTH_DEFAULT_TIMEOUT_MS;
	pPolicy->ByteLimit = XSSH_AUTH_DEFAULT_BYTE_LIMIT;
	pPolicy->AttemptLimit = XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT;
	pPolicy->RoundLimit = XSSH_AUTH_DEFAULT_ROUND_LIMIT;
	pPolicy->MessageLimit = XSSH_AUTH_DEFAULT_MESSAGE_LIMIT;
}



/* 开始一个不拥有时钟的认证预算会话。 */
bool xrtSshAuthGuardInit(
	xsshauthguard* pGuard,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
)
{
	xsshauthguardpolicy Policy;
	xsshauthguard Guard;

	if ( pGuard == NULL ) {
		return false;
	}
	if ( pPolicy == NULL ) {
		xrtSshAuthGuardPolicyInit(&Policy);
		pPolicy = &Policy;
	}
	if ( xrtMemRangesOverlap(
		pGuard,
		sizeof(*pGuard),
		pPolicy,
		sizeof(*pPolicy)
	) ) {
		return false;
	}
	memset(&Guard, 0, sizeof(Guard));
	Guard.Policy = *pPolicy;
	Guard.StartedMs = iNowMs;
	Guard.Initialized = true;
	*pGuard = Guard;
	return true;
}



/* 查询当前认证预算，不增加任何计数。 */
xsshcode xrtSshAuthGuardCheck(
	xsshauthguard* pGuard,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
)
{
	xsshauthexhaustion Exhaustion;

	if ( !xsshAuthGuardValid(pGuard) || (pDecision == NULL) ||
		xrtMemRangesOverlap(
			pGuard,
			sizeof(*pGuard),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pGuard->Complete ) {
		*pDecision = XSSH_AUTH_GUARD_IGNORE;
		return XSSH_OK;
	}
	Exhaustion = xsshAuthGuardCurrent(pGuard, iNowMs);
	if ( Exhaustion != XSSH_AUTH_EXHAUST_NONE ) {
		pGuard->Exhaustion = Exhaustion;
		*pDecision = XSSH_AUTH_GUARD_DISCONNECT;
		return XSSH_OK;
	}
	*pDecision = XSSH_AUTH_GUARD_ALLOW;
	return XSSH_OK;
}



/* 原子计入一条消息并在超限时冻结断开原因。 */
xsshcode xrtSshAuthGuardReserve(
	xsshauthguard* pGuard,
	xsshauthevent Event,
	uint64 iMessageBytes,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
)
{
	xsshauthguard Guard;
	xsshauthguarddecision Decision;
	xsshcode Code;

	if ( !xsshAuthGuardValid(pGuard) || (pDecision == NULL) ||
		(Event < XSSH_AUTH_EVENT_MESSAGE) ||
		(Event > XSSH_AUTH_EVENT_ROUND) ||
		xrtMemRangesOverlap(
			pGuard,
			sizeof(*pGuard),
			pDecision,
			sizeof(*pDecision)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshAuthGuardCheck(pGuard, iNowMs, &Decision);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( Decision != XSSH_AUTH_GUARD_ALLOW ) {
		*pDecision = Decision;
		return XSSH_OK;
	}
	Guard = *pGuard;
	Guard.Messages = xsshAuthGuardIncrement(Guard.Messages);
	Guard.Bytes = xsshAuthGuardAdd64(Guard.Bytes, iMessageBytes);
	if ( Event == XSSH_AUTH_EVENT_ATTEMPT ) {
		Guard.Attempts = xsshAuthGuardIncrement(Guard.Attempts);
	} else if ( Event == XSSH_AUTH_EVENT_ROUND ) {
		Guard.Rounds = xsshAuthGuardIncrement(Guard.Rounds);
	}
	Guard.Exhaustion = xsshAuthGuardCurrent(&Guard, iNowMs);
	*pGuard = Guard;
	*pDecision = Guard.Exhaustion == XSSH_AUTH_EXHAUST_NONE ?
		XSSH_AUTH_GUARD_ALLOW : XSSH_AUTH_GUARD_DISCONNECT;
	return XSSH_OK;
}



/* 成功状态只能从仍可用的认证预算进入。 */
bool xrtSshAuthGuardComplete(xsshauthguard* pGuard)
{
	if ( !xsshAuthGuardValid(pGuard) ||
		(pGuard->Exhaustion != XSSH_AUTH_EXHAUST_NONE) ) {
		return false;
	}
	pGuard->Complete = true;
	return true;
}

#endif
