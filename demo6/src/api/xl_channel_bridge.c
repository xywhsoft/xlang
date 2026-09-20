#include <stdint.h>

#define XRT_MODULE_CHANNEL_SELECT_CANCEL
#define XRT_MODULE_CHANNEL_COROUTINE
#include "lib/xrt/xrt.h"

#include "xl_channel_bridge.h"



/* 把语言层毫秒相对时间转换为 XRT 微秒。 */
static uint64 __xl6ChannelTimeout(long long iMilliseconds)
{
	if ( iMilliseconds <= 0 ) {
		return 0;
	}
	if ( (unsigned long long)iMilliseconds >
		(UINT64_MAX / UINT64_C(1000)) ) {
		return UINT64_MAX;
	}
	return (uint64)iMilliseconds * UINT64_C(1000);
}



/* 把非阻塞 Channel 状态映射为语言层结果。 */
static xl6_channel_result __xl6ChannelTryResult(xchannelresult iResult)
{
	switch ( iResult ) {
		case XCHANNEL_OK:
			return XL6_CHANNEL_OK;

		case XCHANNEL_EMPTY:
			return XL6_CHANNEL_EMPTY;

		case XCHANNEL_FULL:
			return XL6_CHANNEL_FULL;

		case XCHANNEL_CLOSED:
			return XL6_CHANNEL_CLOSED;

		default:
			return XL6_CHANNEL_ERROR;
	}
}



/* 把可等待操作状态映射为语言层结果。 */
static xl6_channel_result __xl6ChannelWaitResult(xwaitresult iResult)
{
	switch ( iResult ) {
		case XWAIT_OK:
			return XL6_CHANNEL_OK;

		case XWAIT_TIMEOUT:
			return XL6_CHANNEL_TIMEOUT;

		case XWAIT_CLOSED:
			return XL6_CHANNEL_CLOSED;

		default:
			return XL6_CHANNEL_ERROR;
	}
}



/* 创建由当前 XRT 实现承载的 Channel；语言层默认保留至少两个缓冲槽。 */
XL_API void* xl6ChannelCreate(size_t iCapacity)
{
	return xrtChannelCreate(iCapacity < 2 ? 2 : iCapacity);
}



/* 销毁已经关闭且不再被任务访问的 Channel。 */
XL_API void xl6ChannelDestroy(void* pChannel)
{
	if ( pChannel != NULL ) {
		(void)xrtChannelDestroy((xchannel*)pChannel);
	}
}



/* 按调用点要求执行阻塞或非阻塞发送。 */
XL_API xl6_channel_result xl6ChannelSend(
	void* pChannel,
	void* pItem,
	bool bWait
)
{
	if ( pChannel == NULL ) {
		return XL6_CHANNEL_ERROR;
	}
	if ( bWait ) {
		return __xl6ChannelWaitResult(
			xrtChannelSend((xchannel*)pChannel, pItem)
		);
	}
	return __xl6ChannelTryResult(
		xrtChannelTrySend((xchannel*)pChannel, pItem)
	);
}



/* 按调用点要求执行阻塞或非阻塞接收。 */
XL_API xl6_channel_result xl6ChannelRecv(
	void* pChannel,
	void** pItem,
	bool bWait
)
{
	if ( (pChannel == NULL) || (pItem == NULL) ) {
		return XL6_CHANNEL_ERROR;
	}
	if ( bWait ) {
		return __xl6ChannelWaitResult(
			xrtChannelRecv((xchannel*)pChannel, pItem)
		);
	}
	return __xl6ChannelTryResult(
		xrtChannelTryRecv((xchannel*)pChannel, pItem)
	);
}



/* 在语言层毫秒超时内等待接收。 */
XL_API xl6_channel_result xl6ChannelRecvFor(
	void* pChannel,
	void** pItem,
	long long iTimeoutMs
)
{
	if ( (pChannel == NULL) || (pItem == NULL) ) {
		return XL6_CHANNEL_ERROR;
	}
	return __xl6ChannelWaitResult(
		xrtChannelRecvFor(
			(xchannel*)pChannel,
			pItem,
			__xl6ChannelTimeout(iTimeoutMs)
		)
	);
}



/* 幂等关闭发送端，保留已经排队的值供接收端排空。 */
XL_API void xl6ChannelClose(void* pChannel)
{
	if ( pChannel != NULL ) {
		xrtChannelClose((xchannel*)pChannel);
	}
}



/* 返回当前缓冲值数量。 */
XL_API size_t xl6ChannelCount(void* pChannel)
{
	return pChannel != NULL ?
		xrtChannelCount((xchannel*)pChannel) : 0;
}



/* 判断发送端是否已经关闭。 */
XL_API bool xl6ChannelIsClosed(void* pChannel)
{
	return (pChannel != NULL) &&
		xrtChannelIsClosed((xchannel*)pChannel);
}
