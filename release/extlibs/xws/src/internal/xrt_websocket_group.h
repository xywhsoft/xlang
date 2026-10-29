#ifndef XRT_INTERNAL_WEBSOCKET_GROUP_H
#define XRT_INTERNAL_WEBSOCKET_GROUP_H

#include "xrt_websocket.h"
#include "../../include/xrt/websocket_group.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP)

/* 验证 Group 固定结构并设置稳定的 Group 域错误。 */
bool __xrtWsGroupCheck(
	const xwsgroup* pGroup,
	cstr sOperation
);



/* 判断一个已验证范围是否覆盖 Group 的私有固定结构。 */
bool __xrtWsGroupOverlaps(
	const xwsgroup* pGroup,
	cbytes pData,
	size_t iSize
);



/* 设置带稳定 WebSocket Group 域的结构化错误。 */
void __xrtWsGroupError(
	xerrkind Kind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);



/* 为底层失败补充稳定的连接组操作边界。 */
void __xrtWsGroupWrap(
	xerrkind DefaultKind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);

#endif

#endif
