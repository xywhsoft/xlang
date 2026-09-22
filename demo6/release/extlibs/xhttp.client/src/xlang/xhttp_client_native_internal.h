#ifndef XLANG_EXTLIB_XHTTP_CLIENT_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_CLIENT_NATIVE_INTERNAL_H

#include "../../include/xhttp_client_package.h"

#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_CLIENT_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_CLIENT_EXPORT
#endif



/* 请求计划句柄允许 Exchange 原子接管底层所有权。 */
typedef struct xhttp_client_plan_handle {
	xhttp1requestplan* Plan;
} xhttp_client_plan_handle;



/* Exchange 句柄保存最近一次非阻塞输出和输入结果。 */
typedef struct xhttp_client_exchange_handle {
	xhttp1exchange* Exchange;
	xhttp1outputstatus OutputStatus;
	xhttp1feedstatus FeedStatus;
	size_t FeedAccepted;
} xhttp_client_exchange_handle;



/* 构造 xhttp.client 语言桥接自己的结构化错误。 */
void xhttpClientNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 检查 Request 句柄并发布一致的参数错误。 */
xhttprequest* xhttpClientRequestRequire(void* pHandle);



/* 检查只读响应句柄并发布一致的参数错误。 */
xhttpresponse* xhttpClientResponseRequire(void* pHandle);



/* 检查尚未被 Exchange 接管的请求计划。 */
xhttp1requestplan* xhttpClientPlanRequire(void* pHandle);



/* 检查 HTTP/1 Exchange 句柄。 */
xhttp1exchange* xhttpClientExchangeRequire(void* pHandle);



/* 把借用文本复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpClientNativeBuffer(xstrview Text);



/* 把借用二进制视图复制到稳定的 xlang Buffer 句柄。 */
xbuffer* xhttpClientNativeBytes(xbytesview Data);



#endif
