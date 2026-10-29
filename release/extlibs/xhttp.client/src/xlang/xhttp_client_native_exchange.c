#include "xhttp_client_native_internal.h"



/* 冻结 Request 并创建可独立检查的 HTTP/1 请求计划。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_plan_prepare(
	void* pRequestHandle,
	long long iTargetForm,
	const char* sCustomTarget,
	size_t iCustomTarget
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pRequestHandle);
	xhttp_client_plan_handle* pHandle;
	xhttp1requestoptions Options;

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( (iTargetForm < (long long)XHTTP1_TARGET_AUTO) ||
		(iTargetForm > (long long)XHTTP1_TARGET_CUSTOM) ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"prepare-http1-plan",
			"HTTP/1 request target form is invalid"
		);
		return NULL;
	}
	pHandle = (xhttp_client_plan_handle*)xrtMalloc(sizeof(*pHandle));
	if ( pHandle == NULL ) {
		return NULL;
	}
	xrtHttp1RequestOptionsInit(&Options);
	Options.TargetForm = (xhttp1targetform)iTargetForm;
	Options.CustomTarget = iCustomTarget != 0u ?
		(xstrview){ sCustomTarget, iCustomTarget } :
		(xstrview){ NULL, 0u };
	pHandle->Plan = xrtHttp1RequestPrepare(pRequest, &Options);
	if ( pHandle->Plan == NULL ) {
		xrtFree(pHandle);
		return NULL;
	}
	return pHandle;
}



/* point 构造接管本包创建的计划持有器。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_plan_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放尚未被 Exchange 接管的请求计划。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_plan_destroy(
	void* pHandle
)
{
	xhttp_client_plan_handle* pPlan =
		(xhttp_client_plan_handle*)pHandle;

	if ( pPlan != NULL ) {
		xrtHttp1RequestPlanDestroy(pPlan->Plan);
		xrtFree(pPlan);
	}
}



/* 返回底层请求计划；计划被接管后返回空指针。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_plan_native(void* pHandle)
{
	xhttp_client_plan_handle* pPlan =
		(xhttp_client_plan_handle*)pHandle;

	return pPlan != NULL ? pPlan->Plan : NULL;
}



/* 复制请求计划拥有的文本或线路字节。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_plan_text(
	void* pHandle,
	long long iField
)
{
	xhttp1requestplan* pPlan = xhttpClientPlanRequire(pHandle);
	xbytesview Bytes;

	if ( pPlan == NULL ) {
		return NULL;
	}
	switch ( iField ) {
		case 0:
			Bytes = xrtHttp1RequestPlanHead(pPlan);
			break;
		case 1:
			Bytes = xrtHttp1RequestPlanEnd(pPlan);
			break;
		case 2:
			return xhttpClientNativeBuffer(
				xrtHttp1RequestPlanMethod(pPlan)
			);
		case 3:
			return xhttpClientNativeBuffer(
				xrtHttp1RequestPlanUrl(pPlan)
			);
		case 4:
			return xhttpClientNativeBuffer(
				xrtHttp1RequestPlanTarget(pPlan)
			);
		case 5:
			return xhttpClientNativeBuffer(
				xrtHttp1RequestPlanHost(pPlan)
			);
		default:
			xhttpClientNativeError(
				XERR_ARGUMENT,
				"read-http1-plan-text",
				"HTTP/1 request plan text selector is invalid"
			);
			return NULL;
	}
	return xhttpClientNativeBytes(Bytes);
}



/* 返回请求计划的整数元数据，未知正文长度使用 -1。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_plan_value(
	void* pHandle,
	long long iField
)
{
	xhttp1requestplan* pPlan = xhttpClientPlanRequire(pHandle);
	uint64 iLength;

	if ( pPlan == NULL ) {
		return 0;
	}
	switch ( iField ) {
		case 0:
			return (long long)xrtHttp1RequestPlanPort(pPlan);
		case 1:
			return (long long)xrtHttp1RequestPlanBodyMode(pPlan);
		case 2:
			iLength = xrtHttp1RequestPlanBodyLength(pPlan);
			return iLength == XHTTP_BODY_UNKNOWN ?
				-1 : (long long)iLength;
		case 3:
			return xrtHttp1RequestPlanSecure(pPlan) ? 1 : 0;
		case 4:
			return xrtHttp1RequestPlanClose(pPlan) ? 1 : 0;
		case 5:
			return xrtHttp1RequestPlanExpectContinue(pPlan) ? 1 : 0;
		default:
			xhttpClientNativeError(
				XERR_ARGUMENT,
				"read-http1-plan-value",
				"HTTP/1 request plan value selector is invalid"
			);
			return 0;
	}
}



/* 返回请求计划保留的独立正文引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_plan_body(void* pHandle)
{
	xhttp1requestplan* pPlan = xhttpClientPlanRequire(pHandle);
	xhttpbody* pBody;

	if ( pPlan == NULL ) {
		return NULL;
	}
	pBody = xrtHttp1RequestPlanBody(pPlan);
	return pBody != NULL ? xrtHttpBodyRef(pBody) : NULL;
}



/* 原子接管请求计划并创建无 I/O HTTP/1 Exchange。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_create(
	void* pPlanHandle
)
{
	xhttp_client_plan_handle* pPlan =
		(xhttp_client_plan_handle*)pPlanHandle;
	xhttp_client_exchange_handle* pHandle;
	xhttp1exchange* pExchange;

	if ( xhttpClientPlanRequire(pPlanHandle) == NULL ) {
		return NULL;
	}
	pHandle = (xhttp_client_exchange_handle*)xrtMalloc(sizeof(*pHandle));
	if ( pHandle == NULL ) {
		return NULL;
	}
	pExchange = xrtHttp1ExchangeCreate(pPlan->Plan, NULL, NULL);
	if ( pExchange == NULL ) {
		xrtFree(pHandle);
		return NULL;
	}
	pPlan->Plan = NULL;
	pHandle->Exchange = pExchange;
	pHandle->OutputStatus = XHTTP1_OUTPUT_AGAIN;
	pHandle->FeedStatus = XHTTP1_FEED_MORE;
	pHandle->FeedAccepted = 0u;
	return pHandle;
}



/* point 构造接管 Exchange 持有器。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 Exchange、未取走的 Response 和其请求计划。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_exchange_destroy(
	void* pHandle
)
{
	xhttp_client_exchange_handle* pExchange =
		(xhttp_client_exchange_handle*)pHandle;

	if ( pExchange != NULL ) {
		xrtHttp1ExchangeDestroy(pExchange->Exchange);
		xrtFree(pExchange);
	}
}



/* 返回可传给 XRT C API 的底层 Exchange。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_native(
	void* pHandle
)
{
	xhttp_client_exchange_handle* pExchange =
		(xhttp_client_exchange_handle*)pHandle;

	return pExchange != NULL ? pExchange->Exchange : NULL;
}



/* 借出下一段线路数据并复制为稳定 Buffer，不自动确认消费。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_output(
	void* pHandle,
	size_t iMaxBytes
)
{
	xhttp_client_exchange_handle* pHandleState =
		(xhttp_client_exchange_handle*)pHandle;
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);
	xbytesview Data;

	if ( pExchange == NULL ) {
		return NULL;
	}
	pHandleState->OutputStatus = xrtHttp1ExchangeOutput(
		pExchange, iMaxBytes, &Data
	);
	return pHandleState->OutputStatus == XHTTP1_OUTPUT_DATA ?
		xhttpClientNativeBytes(Data) : NULL;
}



/* 返回最近一次 Output 的状态。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_exchange_output_status(
	void* pHandle
)
{
	xhttp_client_exchange_handle* pExchange =
		(xhttp_client_exchange_handle*)pHandle;

	return pExchange != NULL ?
		(long long)pExchange->OutputStatus :
		(long long)XHTTP1_OUTPUT_ERROR;
}



/* 确认最近一次线路输出中已经发送的前缀。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_exchange_consume(
	void* pHandle,
	size_t iSize
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	return (pExchange != NULL) &&
		xrtHttp1ExchangeOutputConsume(pExchange, iSize);
}



/* 允许 Expect: 100-continue 请求开始发送正文。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_exchange_continue(
	void* pHandle
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	return (pExchange != NULL) && xrtHttp1ExchangeContinue(pExchange);
}



/* 暂停或恢复响应输入交付。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_exchange_pause(
	void* pHandle,
	bool bPause
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	if ( pExchange == NULL ) {
		return false;
	}
	return bPause ?
		xrtHttp1ExchangePause(pExchange) :
		xrtHttp1ExchangeResume(pExchange);
}



/* 判断响应输入是否暂停。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_exchange_paused(
	void* pHandle
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	return (pExchange != NULL) && xrtHttp1ExchangePaused(pExchange);
}



/* 同步输入一段响应线路字节并保存状态与已接收前缀。 */
XLANG_XHTTP_CLIENT_EXPORT unsigned long long xhttp_client_exchange_feed(
	void* pHandle,
	void* pData,
	size_t iSize,
	bool bEnd
)
{
	xhttp_client_exchange_handle* pHandleState =
		(xhttp_client_exchange_handle*)pHandle;
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	if ( pExchange == NULL ) {
		return 0u;
	}
	pHandleState->FeedAccepted = 0u;
	pHandleState->FeedStatus = xrtHttp1ExchangeFeed(
		pExchange,
		(xbytesview){ (cbytes)pData, iSize },
		bEnd,
		&pHandleState->FeedAccepted
	);
	return (unsigned long long)pHandleState->FeedAccepted;
}



/* 返回最近一次 Feed 的状态。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_exchange_feed_status(
	void* pHandle
)
{
	xhttp_client_exchange_handle* pExchange =
		(xhttp_client_exchange_handle*)pHandle;

	return pExchange != NULL ?
		(long long)pExchange->FeedStatus :
		(long long)XHTTP1_FEED_ERROR;
}



/* 在终态后取走拥有型 Response。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_take_response(
	void* pHandle
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	return pExchange != NULL ?
		xrtHttp1ExchangeTakeResponse(pExchange) : NULL;
}



/* 复制升级或跨报文边界保留的输入后缀。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_exchange_remainder(
	void* pHandle
)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	return pExchange != NULL ?
		xhttpClientNativeBytes(xrtHttp1ExchangeRemainder(pExchange)) : NULL;
}



/* 返回 Exchange 的布尔与计数状态。 */
XLANG_XHTTP_CLIENT_EXPORT unsigned long long
xhttp_client_exchange_value(void* pHandle, long long iField)
{
	xhttp1exchange* pExchange = xhttpClientExchangeRequire(pHandle);

	if ( pExchange == NULL ) {
		return 0u;
	}
	switch ( iField ) {
		case 0:
			return xrtHttp1ExchangeRequestComplete(pExchange) ? 1u : 0u;
		case 1:
			return xrtHttp1ExchangeResponseComplete(pExchange) ? 1u : 0u;
		case 2:
			return xrtHttp1ExchangeUpgraded(pExchange) ? 1u : 0u;
		case 3:
			return xrtHttp1ExchangeReusable(pExchange) ? 1u : 0u;
		case 4:
			return (unsigned long long)
				xrtHttp1ExchangeInformationalCount(pExchange);
		case 5:
			return (unsigned long long)
				xrtHttp1ExchangeRequestWireBytes(pExchange);
		default:
			xhttpClientNativeError(
				XERR_ARGUMENT,
				"read-http1-exchange-value",
				"HTTP/1 Exchange value selector is invalid"
			);
			return 0u;
	}
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_plan_destroy_abi(void* handle)
{
    (void)xhttp_client_plan_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_exchange_destroy_abi(void* handle)
{
    (void)xhttp_client_exchange_destroy(handle);
}
