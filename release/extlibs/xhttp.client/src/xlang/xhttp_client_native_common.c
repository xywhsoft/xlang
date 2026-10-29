#include "xhttp_client_native_internal.h"



/* 构造统一使用 xlang.extlib.xhttp.client 域的结构化错误。 */
void xhttpClientNativeError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xlang.extlib.xhttp.client";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	} else if ( xrtGetError() == NULL ) {
		xrtSetErrorKind(Kind);
	}
}



/* 检查 Request 句柄并发布一致的参数错误。 */
xhttprequest* xhttpClientRequestRequire(void* pHandle)
{
	if ( pHandle == NULL ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"access-request",
			"Request handle is invalid"
		);
		return NULL;
	}
	return (xhttprequest*)pHandle;
}



/* 检查 Response 句柄并发布一致的参数错误。 */
xhttpresponse* xhttpClientResponseRequire(void* pHandle)
{
	if ( pHandle == NULL ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"access-response",
			"Response handle is invalid"
		);
		return NULL;
	}
	return (xhttpresponse*)pHandle;
}



/* 检查计划句柄及其尚未转移的底层计划。 */
xhttp1requestplan* xhttpClientPlanRequire(void* pHandle)
{
	xhttp_client_plan_handle* pPlan =
		(xhttp_client_plan_handle*)pHandle;

	if ( (pPlan == NULL) || (pPlan->Plan == NULL) ) {
		xhttpClientNativeError(
			XERR_STATE,
			"access-http1-plan",
			"HTTP/1 request plan has been consumed or is invalid"
		);
		return NULL;
	}
	return pPlan->Plan;
}



/* 检查 Exchange 句柄及其底层状态机。 */
xhttp1exchange* xhttpClientExchangeRequire(void* pHandle)
{
	xhttp_client_exchange_handle* pExchange =
		(xhttp_client_exchange_handle*)pHandle;

	if ( (pExchange == NULL) || (pExchange->Exchange == NULL) ) {
		xhttpClientNativeError(
			XERR_STATE,
			"access-http1-exchange",
			"HTTP/1 Exchange handle is invalid"
		);
		return NULL;
	}
	return pExchange->Exchange;
}



/* 把借用文本复制到稳定 Buffer。 */
xbuffer* xhttpClientNativeBuffer(xstrview Text)
{
	return xhttpClientNativeBytes((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* 把借用二进制视图复制到稳定 Buffer。 */
xbuffer* xhttpClientNativeBytes(xbytesview Data)
{
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"copy-http-client-bytes",
			"HTTP client byte view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 返回当前线程错误文本的独立副本。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp.client operation failed";
	}
	return xhttpClientNativeBuffer((xstrview){
		sMessage,
		strlen(sMessage)
	});
}



/* 二进制输入原样保留；文本有终止空间后移交原分配。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_buffer_take_cstr(void* handle)
{
	xbuffer* buffer = (xbuffer*)handle;
	if (buffer == NULL || buffer->Size == SIZE_MAX ||
		(buffer->Size != 0u && buffer->Data == NULL) ||
		(buffer->Size != 0u && memchr(buffer->Data, 0, buffer->Size) != NULL) ||
		!xrtBufferReserve(buffer, buffer->Size + 1u)) return NULL;
	buffer->Data[buffer->Size] = 0;
	return xrtBufferTake(buffer, NULL, NULL);
}
