#include <string.h>

#include <xrt/ssh_channel_core.h>



#if defined(XSSH_FEATURE_CHANNEL_CORE)

/* 校验公开状态没有被未初始化对象或越界阶段伪造。 */
static bool xsshChannelCoreValid(const xsshchannelcore* pChannel)
{
	return xrtMemRangeValid(pChannel, sizeof(*pChannel)) &&
		pChannel->Initialized &&
		(pChannel->Phase >= XSSH_CHANNEL_CORE_OPENING) &&
		(pChannel->Phase <= XSSH_CHANNEL_CORE_CLOSED);
}



/* 打开数据面的阶段必须同时具有有效生命周期状态。 */
static bool xsshChannelCoreDataValid(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreValid(pChannel) &&
		((pChannel->Phase == XSSH_CHANNEL_CORE_OPEN) ||
		 (pChannel->Phase == XSSH_CHANNEL_CORE_CLOSED)) &&
		pChannel->State.Initialized;
}



/* 判断输入结构与 channel 对象互不覆盖。 */
static bool xsshChannelCoreInputValid(
	const xsshchannelcore* pChannel,
	const void* pInput,
	size_t iInputSize
)
{
	return xrtMemRangeValid(pInput, iInputSize) &&
		!xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pInput,
			iInputSize
		);
}



/* 双向 close 完成后冻结协议状态，但保留窗口中的应用缓冲计数。 */
static void xsshChannelCoreCloseUpdate(xsshchannelcore* pChannel)
{
	if ( xrtSshChannelClosed(&pChannel->State) ) {
		pChannel->Phase = XSSH_CHANNEL_CORE_CLOSED;
	}
}



/* 初始化等待 confirmation 的本端 open。 */
bool xrtSshChannelCoreOpenInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelcore Channel;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		return false;
	}
	memset(&Channel, 0, sizeof(Channel));
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		0u,
		1u,
		iReceiveWindow,
		iReceiveMaxPacket,
		iAdjustThreshold
	) ) {
		return false;
	}
	Channel.Local = iLocal;
	Channel.Phase = XSSH_CHANNEL_CORE_OPENING;
	Channel.Initialized = true;
	*pChannel = Channel;
	return true;
}



/* 初始化等待本端确认的 peer open。 */
bool xrtSshChannelCoreAcceptInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelcore Channel;

	if ( !xrtMemRangeValid(pChannel, sizeof(*pChannel)) ||
		!xrtMemRangeValid(pOpen, sizeof(*pOpen)) ||
		xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pOpen,
			sizeof(*pOpen)
		) || (pOpen->MaxPacket == 0u) ) {
		return false;
	}
	memset(&Channel, 0, sizeof(Channel));
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		pOpen->Window,
		pOpen->MaxPacket,
		iReceiveWindow,
		iReceiveMaxPacket,
		iAdjustThreshold
	) ) {
		return false;
	}
	Channel.Local = iLocal;
	Channel.Remote = pOpen->Sender;
	Channel.Phase = XSSH_CHANNEL_CORE_ACCEPTING;
	Channel.Initialized = true;
	*pChannel = Channel;
	return true;
}



/* 清除不拥有外部资源的 channel core。 */
void xrtSshChannelCoreClear(xsshchannelcore* pChannel)
{
	if ( xrtMemRangeValid(pChannel, sizeof(*pChannel)) ) {
		memset(pChannel, 0, sizeof(*pChannel));
	}
}



/* 查询公开阶段。 */
xsshchannelcorephase xrtSshChannelCorePhase(
	const xsshchannelcore* pChannel
)
{
	return xsshChannelCoreValid(pChannel) ?
		pChannel->Phase : XSSH_CHANNEL_CORE_FAILED;
}



/* 复制 channel 两个方向的线路编号。 */
bool xrtSshChannelCoreIds(
	const xsshchannelcore* pChannel,
	uint32* pLocal,
	uint32* pRemote
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		((pChannel->Phase == XSSH_CHANNEL_CORE_OPENING) ||
		 (pChannel->Phase == XSSH_CHANNEL_CORE_FAILED)) ||
		!xrtMemRangeValid(pLocal, sizeof(*pLocal)) ||
		!xrtMemRangeValid(pRemote, sizeof(*pRemote)) ||
		xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pLocal,
			sizeof(*pLocal)
		) || xrtMemRangesOverlap(
			pChannel,
			sizeof(*pChannel),
			pRemote,
			sizeof(*pRemote)
		) || xrtMemRangesOverlap(
			pLocal,
			sizeof(*pLocal),
			pRemote,
			sizeof(*pRemote)
		) ) {
		return false;
	}
	*pLocal = pChannel->Local;
	*pRemote = pChannel->Remote;
	return true;
}



/* 原子应用 peer confirmation。 */
xsshcode xrtSshChannelCoreConfirmationCommit(
	xsshchannelcore* pChannel,
	const xsshchannelconfirmation* pConfirmation
)
{
	xsshchannelcore Channel;

	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelCoreInputValid(
		pChannel,
		pConfirmation,
		sizeof(*pConfirmation)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (pConfirmation->Recipient != pChannel->Local) ||
		(pConfirmation->MaxPacket == 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Channel = *pChannel;
	if ( !xrtSshChannelWindowInit(
		&Channel.Window,
		pConfirmation->Window,
		pConfirmation->MaxPacket,
		pChannel->Window.ReceiveWindow,
		pChannel->Window.ReceiveMaxPacket,
		pChannel->Window.AdjustThreshold
	) || !xrtSshChannelStateInit(&Channel.State) ) {
		return XSSH_ERROR_STATE;
	}
	Channel.Remote = pConfirmation->Sender;
	Channel.Phase = XSSH_CHANNEL_CORE_OPEN;
	*pChannel = Channel;
	return XSSH_OK;
}



/* 原子应用 peer open failure。 */
xsshcode xrtSshChannelCoreFailureCommit(
	xsshchannelcore* pChannel,
	const xsshchannelopenfailure* pFailure
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_OPENING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xsshChannelCoreInputValid(
		pChannel,
		pFailure,
		sizeof(*pFailure)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pFailure->Recipient != pChannel->Local ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pChannel->FailureReason = pFailure->Reason;
	pChannel->Phase = XSSH_CHANNEL_CORE_FAILED;
	return XSSH_OK;
}



/* 本端 confirmation 可靠提交后开放输入 channel。 */
xsshcode xrtSshChannelCoreAcceptCommit(xsshchannelcore* pChannel)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_ACCEPTING) ) {
		return XSSH_ERROR_STATE;
	}
	if ( !xrtSshChannelStateInit(&pChannel->State) ) {
		return XSSH_ERROR_STATE;
	}
	pChannel->Phase = XSSH_CHANNEL_CORE_OPEN;
	return XSSH_OK;
}



/* 本端 failure 可靠提交后冻结拒绝结果。 */
xsshcode xrtSshChannelCoreRejectCommit(
	xsshchannelcore* pChannel,
	uint32 iReason
)
{
	if ( !xsshChannelCoreValid(pChannel) ||
		(pChannel->Phase != XSSH_CHANNEL_CORE_ACCEPTING) ) {
		return XSSH_ERROR_STATE;
	}
	pChannel->FailureReason = iReason;
	pChannel->Phase = XSSH_CHANNEL_CORE_FAILED;
	return XSSH_OK;
}



/* 判断 channel 数据面已开放。 */
bool xrtSshChannelCoreOpen(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreDataValid(pChannel) &&
		(pChannel->Phase == XSSH_CHANNEL_CORE_OPEN);
}



/* 判断双向 close 已完成。 */
bool xrtSshChannelCoreClosed(const xsshchannelcore* pChannel)
{
	return xsshChannelCoreDataValid(pChannel) &&
		(pChannel->Phase == XSSH_CHANNEL_CORE_CLOSED) &&
		xrtSshChannelClosed(&pChannel->State);
}



/* 查询当前数据发送上限。 */
uint32 xrtSshChannelCoreSendLimit(const xsshchannelcore* pChannel)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ||
		!xrtSshChannelCanSendData(&pChannel->State) ) {
		return 0u;
	}
	return xrtSshChannelSendLimit(&pChannel->Window);
}



/* 提交本端 data 的窗口消费。 */
xsshcode xrtSshChannelCoreDataSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ||
		!xrtSshChannelCanSendData(&pChannel->State) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelSendCommit(&pChannel->Window, iBytes);
}



/* 提交 peer data 的生命周期与窗口消费。 */
xsshcode xrtSshChannelCoreDataReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient,
	uint32 iBytes
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtSshChannelCanReceiveData(&pChannel->State) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xrtSshChannelReceiveCommit(&pChannel->Window, iBytes);
}



/* 消费调用方已经处理的数据。 */
xsshcode xrtSshChannelCoreDataConsume(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xsshChannelCoreDataValid(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelReceiveConsume(&pChannel->Window, iBytes);
}



/* 判断仍可发送窗口更新且返还额度达到阈值。 */
bool xrtSshChannelCoreAdjustReady(const xsshchannelcore* pChannel)
{
	return xrtSshChannelCoreCanSendRequest(pChannel) &&
		xrtSshChannelReceiveAdjustReady(&pChannel->Window);
}



/* 返回当前可返还额度。 */
uint32 xrtSshChannelCoreAdjustLimit(const xsshchannelcore* pChannel)
{
	return xrtSshChannelCoreCanSendRequest(pChannel) ?
		xrtSshChannelReceiveAdjustLimit(&pChannel->Window) : 0u;
}



/* 提交本端已可靠发送的窗口更新。 */
xsshcode xrtSshChannelCoreAdjustSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
)
{
	if ( !xrtSshChannelCoreCanSendRequest(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelReceiveAdjustCommit(&pChannel->Window, iBytes);
}



/* 提交 peer 窗口更新。 */
xsshcode xrtSshChannelCoreAdjustReceiveCommit(
	xsshchannelcore* pChannel,
	const xsshchanneladjust* pAdjust
)
{
	xsshcode Code;

	if ( !xsshChannelCoreDataValid(pChannel) ||
		!xsshChannelCoreInputValid(
			pChannel,
			pAdjust,
			sizeof(*pAdjust)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		pAdjust->Recipient
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pChannel->State.LocalClose || pChannel->State.RemoteClose ) {
		return XSSH_ERROR_PROTOCOL;
	}
	return xrtSshChannelSendAdjust(&pChannel->Window, pAdjust->Bytes);
}



/* 判断本端仍可发送 request。 */
bool xrtSshChannelCoreCanSendRequest(
	const xsshchannelcore* pChannel
)
{
	return xrtSshChannelCoreOpen(pChannel) &&
		xrtSshChannelCanSendRequest(&pChannel->State);
}



/* 判断 peer 方向仍可到达 request。 */
bool xrtSshChannelCoreCanReceiveRequest(
	const xsshchannelcore* pChannel
)
{
	return xrtSshChannelCoreOpen(pChannel) &&
		!pChannel->State.RemoteClose;
}



/* 校验 recipient 指向当前已打开 channel。 */
xsshcode xrtSshChannelCoreRecipientCheck(
	const xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return iRecipient == pChannel->Local ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 提交本端 EOF。 */
xsshcode xrtSshChannelCoreEofSendCommit(xsshchannelcore* pChannel)
{
	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshChannelLocalEofCommit(&pChannel->State);
}



/* 提交 peer EOF。 */
xsshcode xrtSshChannelCoreEofReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	return Code == XSSH_OK ?
		xrtSshChannelRemoteEofCommit(&pChannel->State) : Code;
}



/* 提交本端 CLOSE，并在握手完成时冻结 channel。 */
xsshcode xrtSshChannelCoreCloseSendCommit(xsshchannelcore* pChannel)
{
	xsshcode Code;

	if ( !xrtSshChannelCoreOpen(pChannel) ) {
		return XSSH_ERROR_STATE;
	}
	Code = xrtSshChannelLocalCloseCommit(&pChannel->State);
	if ( Code == XSSH_OK ) {
		xsshChannelCoreCloseUpdate(pChannel);
	}
	return Code;
}



/* 提交 peer CLOSE，并在握手完成时冻结 channel。 */
xsshcode xrtSshChannelCoreCloseReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
)
{
	xsshcode Code = xrtSshChannelCoreRecipientCheck(
		pChannel,
		iRecipient
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshChannelRemoteCloseCommit(&pChannel->State);
	if ( Code == XSSH_OK ) {
		xsshChannelCoreCloseUpdate(pChannel);
	}
	return Code;
}

#endif
