#ifndef XL_CHANNEL_BRIDGE_H
#define XL_CHANNEL_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include "include/xl.h"



/* xlang 生成代码使用的稳定 Channel 结果，不进入 XRT 公共 ABI。 */
typedef enum xl6_channel_result {
	XL6_CHANNEL_ERROR = -1,
	XL6_CHANNEL_OK = 0,
	XL6_CHANNEL_EMPTY = 1,
	XL6_CHANNEL_FULL = 2,
	XL6_CHANNEL_CLOSED = 3,
	XL6_CHANNEL_TIMEOUT = 4
} xl6_channel_result;



XL_API void* xl6ChannelCreate(size_t iCapacity);
XL_API void xl6ChannelDestroy(void* pChannel);
XL_API xl6_channel_result xl6ChannelSend(
	void* pChannel,
	void* pItem,
	bool bWait
);
XL_API xl6_channel_result xl6ChannelRecv(
	void* pChannel,
	void** pItem,
	bool bWait
);
XL_API xl6_channel_result xl6ChannelRecvFor(
	void* pChannel,
	void** pItem,
	long long iTimeoutMs
);
XL_API void xl6ChannelClose(void* pChannel);
XL_API size_t xl6ChannelCount(void* pChannel);
XL_API bool xl6ChannelIsClosed(void* pChannel);

#endif
