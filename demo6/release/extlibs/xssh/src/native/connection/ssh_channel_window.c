#include <xrt/ssh_channel_window.h>



#if defined(XSSH_FEATURE_CHANNEL_WINDOW)

/* 校验不会被调用方直接修改破坏的固定窗口约束。 */
static bool xsshChannelWindowValid(const xsshchannelwindow* pWindow)
{
	return (pWindow != NULL) && (pWindow->SendMaxPacket != 0u) &&
		(pWindow->ReceiveMaxPacket != 0u) &&
		(pWindow->AdjustThreshold != 0u);
}



/* 初始化纯数值窗口状态。 */
bool xrtSshChannelWindowInit(
	xsshchannelwindow* pWindow,
	uint32 iSendWindow,
	uint32 iSendMaxPacket,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
)
{
	xsshchannelwindow Window;

	if ( (pWindow == NULL) || (iSendMaxPacket == 0u) ||
		(iReceiveMaxPacket == 0u) || (iAdjustThreshold == 0u) ) {
		return false;
	}
	Window.SendWindow = iSendWindow;
	Window.SendMaxPacket = iSendMaxPacket;
	Window.ReceiveWindow = iReceiveWindow;
	Window.ReceiveMaxPacket = iReceiveMaxPacket;
	Window.AdjustThreshold = iAdjustThreshold;
	Window.ReceiveBuffered = 0u;
	Window.ReceivePending = 0u;
	*pWindow = Window;
	return true;
}



/* 返回同时受远端窗口和最大包限制的发送上限。 */
uint32 xrtSshChannelSendLimit(const xsshchannelwindow* pWindow)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return 0u;
	}
	return pWindow->SendWindow < pWindow->SendMaxPacket ?
		pWindow->SendWindow : pWindow->SendMaxPacket;
}



/* 扣减已经可靠排队的发送字节。 */
xsshcode xrtSshChannelSendCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > pWindow->SendMaxPacket ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > pWindow->SendWindow ) {
		return XSSH_ERROR_SPACE;
	}
	pWindow->SendWindow -= iBytes;
	return XSSH_OK;
}



/* 增加远端发送窗口，拒绝 RFC 4254 禁止的 uint32 回绕。 */
xsshcode xrtSshChannelSendAdjust(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > (UINT32_MAX - pWindow->SendWindow) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	pWindow->SendWindow += iBytes;
	return XSSH_OK;
}



/* 校验远端数据并从已通告窗口转入应用缓冲计数。 */
xsshcode xrtSshChannelReceiveCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (iBytes > pWindow->ReceiveMaxPacket) ||
		(iBytes > pWindow->ReceiveWindow) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( (uint64)iBytes > (UINT64_MAX - pWindow->ReceiveBuffered) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pWindow->ReceiveWindow -= iBytes;
	pWindow->ReceiveBuffered += (uint64)iBytes;
	return XSSH_OK;
}



/* 将应用消费量从缓冲计数转入待返还计数。 */
xsshcode xrtSshChannelReceiveConsume(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (uint64)iBytes > pWindow->ReceiveBuffered ) {
		return XSSH_ERROR_STATE;
	}
	if ( (uint64)iBytes > (UINT64_MAX - pWindow->ReceivePending) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	pWindow->ReceiveBuffered -= (uint64)iBytes;
	pWindow->ReceivePending += (uint64)iBytes;
	return XSSH_OK;
}



/* 判断返还额度是否已值得产生一条 WINDOW_ADJUST。 */
bool xrtSshChannelReceiveAdjustReady(const xsshchannelwindow* pWindow)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return false;
	}
	return (pWindow->ReceivePending >=
		(uint64)pWindow->AdjustThreshold) ||
		((pWindow->ReceiveWindow == 0u) &&
		 (pWindow->ReceivePending != 0u));
}



/* 计算不会令当前 uint32 接收窗口回绕的返还额度。 */
uint32 xrtSshChannelReceiveAdjustLimit(
	const xsshchannelwindow* pWindow
)
{
	uint64 iLimit;

	if ( !xsshChannelWindowValid(pWindow) ) {
		return 0u;
	}
	iLimit = (uint64)(UINT32_MAX - pWindow->ReceiveWindow);
	if ( pWindow->ReceivePending < iLimit ) {
		iLimit = pWindow->ReceivePending;
	}
	return (uint32)iLimit;
}



/* 提交已经可靠排队的消费额度返还。 */
xsshcode xrtSshChannelReceiveAdjustCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((uint64)iBytes > pWindow->ReceivePending) ||
		(iBytes > (UINT32_MAX - pWindow->ReceiveWindow)) ) {
		return XSSH_ERROR_STATE;
	}
	pWindow->ReceivePending -= (uint64)iBytes;
	pWindow->ReceiveWindow += iBytes;
	return XSSH_OK;
}



/* 提交新获得的独立接收容量，不消耗待返还计数。 */
xsshcode xrtSshChannelReceiveGrantCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
)
{
	if ( !xsshChannelWindowValid(pWindow) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( iBytes > (UINT32_MAX - pWindow->ReceiveWindow) ) {
		return XSSH_ERROR_STATE;
	}
	pWindow->ReceiveWindow += iBytes;
	return XSSH_OK;
}

#endif
