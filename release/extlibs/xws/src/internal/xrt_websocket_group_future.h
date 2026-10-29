#ifndef XRT_INTERNAL_WEBSOCKET_GROUP_FUTURE_H
#define XRT_INTERNAL_WEBSOCKET_GROUP_FUTURE_H

#include "xrt_websocket.h"
#include <xrt.h>
#include "../../include/xrt/websocket_group.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 成员提交函数只在创建批量操作的调用线程内借用上下文。 */
typedef xfuture* (*__xrt_ws_group_submitproc)(
	xwsconn* pConnection,
	ptr pData
);



/* 创建已经具备稳定成员快照和完成 Future 的批量操作。 */
xwsgroupop* __xrtWsGroupOpCreate(xwsgroup* pGroup);



/* 对稳定快照逐成员提交操作，并保存接纳 Future 或同步错误。 */
bool __xrtWsGroupOpSubmit(
	xwsgroupop* pOperation,
	__xrt_ws_group_submitproc pSubmit,
	ptr pData
);

#endif

#endif
