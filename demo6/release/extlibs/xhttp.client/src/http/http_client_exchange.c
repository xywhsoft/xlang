#include "../internal/xrt_http_exchange.h"



#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)

/* 设置 Exchange 实现使用的通用参数错误。 */
void __xrtHttp1ExchangeInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT,
		"http.exchange",
		XHTTP1_EXCHANGE_ERROR_ARGUMENT,
		"invalid argument"
	);
}



/* 设置 Exchange 实现使用的非法状态错误。 */
void __xrtHttp1ExchangeInvalidState(void)
{
	xrtSetErrorInfo(
		XERR_STATE,
		"http.exchange",
		XHTTP1_EXCHANGE_ERROR_STATE,
		"invalid state"
	);
}



/* 设置 Exchange 实现使用的大小溢出错误。 */
void __xrtHttp1ExchangeSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE,
		"http.exchange",
		XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
		"size overflow"
	);
}



#define XRT_HTTP_EXCHANGE_DEFAULT_BODY UINT64_C(67108864)
#define XRT_HTTP_EXCHANGE_DEFAULT_INFORMATIONAL UINT32_C(16)



/* 验证公开 Exchange 配置的全部硬边界。 */
bool __xrtHttp1ExchangeConfigValid(
	const xhttp1exchangeconfig* pConfig
)
{
	if ( (pConfig->Head.MaxHead < 4) ||
		(pConfig->Head.MaxStartLine == 0) ||
		(pConfig->Head.MaxFieldLine == 0) ||
		(pConfig->Head.MaxStartLine > pConfig->Head.MaxHead) ||
		(pConfig->Head.MaxFieldLine > pConfig->Head.MaxHead) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	return xrtHttpHeadersConfigValid(
		&pConfig->Headers
	) && xrtHttpHeadersConfigValid(
		&pConfig->Trailers
	);
}



/* 验证请求计划包含 Exchange 必需的完整冻结事实。 */
static bool __xrtHttp1ExchangePlanValid(
	const xhttp1requestplan* pPlan
)
{
	xbytesview Head;
	xstrview Method;
	xstrview Url;
	xhttprequestbodymode Mode;

	if ( pPlan == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	Head = xrtHttp1RequestPlanHead(pPlan);
	Method = xrtHttp1RequestPlanMethod(pPlan);
	Url = xrtHttp1RequestPlanUrl(pPlan);
	Mode = xrtHttp1RequestPlanBodyMode(pPlan);
	if ( (Head.Data == NULL) || (Head.Size == 0) ||
		!xrtHttpTokenValid(Method) ||
		(Url.Data == NULL) || (Url.Size == 0) ||
		(Mode < XHTTP_REQUEST_BODY_NONE) ||
		(Mode > XHTTP_REQUEST_BODY_CHUNKED) ||
		((Mode == XHTTP_REQUEST_BODY_FIXED) &&
		 (xrtHttp1RequestPlanBodyLength(pPlan) ==
		  XHTTP_BODY_UNKNOWN)) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	return true;
}



/* 创建 Exchange 自己拥有的错误，并只保留调用方明确提供的原因。 */
bool __xrtHttp1ExchangeFailCause(
	xhttp1exchange* pExchange,
	xhttp1exchangeerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pCauseRef;
	xerror* pError;
	xerrordesc Desc;

	if ( pExchange == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return false;
	}
	pCauseRef = xrtErrorRef(pCause);
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.exchange";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCauseRef;
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCauseRef);
	if ( pError == NULL ) {
		pError = xrtTakeError();
	}
	pExchange->Error = pError;
	pExchange->InputState = XRT_HTTP_EXCHANGE_INPUT_FAILED;
	__xrtHttp1ExchangeStopOutput(pExchange);
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
	}
	return false;
}



/* 创建没有额外原因链的 Exchange 终态错误。 */
bool __xrtHttp1ExchangeFail(
	xhttp1exchange* pExchange,
	xhttp1exchangeerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	return __xrtHttp1ExchangeFailCause(
		pExchange,
		Code,
		Kind,
		sOperation,
		sMessage,
		NULL
	);
}



/* 释放出站正文状态，不释放调用方已经取得的响应。 */
void __xrtHttp1ExchangeStopOutput(
	xhttp1exchange* pExchange
)
{
	if ( pExchange == NULL ) {
		return;
	}
	/*
		已经借出的正文可能仍被异步发送操作引用。
		此时只停止产生新输出，最后一个租约由 Consume 或 Destroy 回收。
	*/
	if ( pExchange->Offered == 0 ) {
		xrtHttpBodyChunkRelease(&pExchange->Chunk);
		xrtHttpBodyReaderDestroy(pExchange->Reader);
		pExchange->Reader = NULL;
	}
	pExchange->OutputAgain = false;
	pExchange->OutputStopped = true;
}



/* 按实际输入建立或扩大临时缓冲。 */
bool __xrtHttp1ExchangeBufferAppend(
	xhttp1exchange* pExchange,
	xrt_http_exchange_buffer* pBuffer,
	xbytesview Data,
	size_t iLimit
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pData;

	if ( (pBuffer == NULL) ||
		((Data.Data == NULL) && (Data.Size != 0)) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"buffer-http1-input",
			"HTTP/1 Exchange input view is invalid"
		);
	}
	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pBuffer->Size > (SIZE_MAX - Data.Size)) ||
		((pBuffer->Size + Data.Size) > iLimit) ) {
		__xrtHttp1ExchangeSizeOverflow();
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
			XERR_RANGE,
			"buffer-http1-input",
			"HTTP/1 Exchange buffered input exceeds its limit"
		);
	}
	iRequired = pBuffer->Size + Data.Size;
	if ( iRequired > pBuffer->Capacity ) {
		iCapacity = pBuffer->Capacity;
		if ( iCapacity == 0 ) {
			iCapacity = iRequired;
		}
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity > (SIZE_MAX / 2) ?
				iRequired : (iCapacity * 2);

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		if ( iCapacity > iLimit ) {
			iCapacity = iLimit;
		}
		pData = (bytes)xrtRealloc(
			pBuffer->Data, iCapacity
		);
		if ( pData == NULL ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
				XERR_MEMORY,
				"buffer-http1-input",
				"HTTP/1 Exchange input buffer allocation failed",
				xrtGetError()
			);
		}
		pBuffer->Data = pData;
		pBuffer->Capacity = iCapacity;
	}
	memcpy(
		pBuffer->Data + pBuffer->Size,
		Data.Data,
		Data.Size
	);
	pBuffer->Size = iRequired;
	return true;
}



/* 移除已经处理的临时输入并保留尚未处理的后缀。 */
void __xrtHttp1ExchangeBufferConsume(
	xrt_http_exchange_buffer* pBuffer,
	size_t iSize
)
{
	if ( (pBuffer == NULL) || (iSize == 0) ) {
		return;
	}
	if ( iSize >= pBuffer->Size ) {
		pBuffer->Size = 0;
		return;
	}
	memmove(
		pBuffer->Data,
		pBuffer->Data + iSize,
		pBuffer->Size - iSize
	);
	pBuffer->Size -= iSize;
}



/* 初始化客户端 Exchange 默认限额。 */
XRT_API void xrtHttp1ExchangeConfigInit(
	xhttp1exchangeconfig* pConfig
)
{
	xhttp1exchangeconfig Config;

	if ( pConfig == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return;
	}
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttp1LimitsInit(&Config.Head);
	xrtHttp1BodyLimitsInit(&Config.Body);
	Config.Body.MaxBody = XRT_HTTP_EXCHANGE_DEFAULT_BODY;
	xrtHttpHeadersConfigInit(&Config.Headers);
	Config.Headers.InitialFields = 0;
	Config.Headers.InitialBytes = 0;
	Config.Headers.MaxFields = Config.Head.MaxFields;
	Config.Headers.MaxName = Config.Head.MaxFieldLine;
	Config.Headers.MaxValue = Config.Head.MaxFieldLine;
	Config.Headers.MaxBytes = Config.Head.MaxHead;
	xrtHttpHeadersConfigInit(&Config.Trailers);
	Config.Trailers.InitialFields = 0;
	Config.Trailers.InitialBytes = 0;
	Config.Trailers.MaxFields = Config.Body.MaxTrailers;
	Config.Trailers.MaxName = Config.Body.MaxTrailerLine;
	Config.Trailers.MaxValue = Config.Body.MaxTrailerLine;
	Config.Trailers.MaxBytes = Config.Body.MaxTrailer;
	Config.MaxInformational =
		XRT_HTTP_EXCHANGE_DEFAULT_INFORMATIONAL;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建并接管一份已经冻结的请求计划。 */
XRT_API xhttp1exchange* xrtHttp1ExchangeCreate(
	xhttp1requestplan* pPlan,
	const xhttp1exchangeconfig* pConfig,
	const xhttp1exchangeevents* pEvents
)
{
	xhttp1exchangeconfig Config;
	xhttp1exchangeevents Events;
	xhttp1exchange* pExchange;

	if ( !__xrtHttp1ExchangePlanValid(pPlan) ) {
		return NULL;
	}
	xrtHttp1ExchangeConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtHttp1ExchangeInvalidArgument();
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtHttp1ExchangeConfigValid(&Config) ) {
		return NULL;
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !xrtMemRangeValid(pEvents, sizeof(*pEvents)) ) {
			__xrtHttp1ExchangeInvalidArgument();
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	pExchange = (xhttp1exchange*)xrtCalloc(
		1, sizeof(*pExchange)
	);
	if ( pExchange == NULL ) {
		return NULL;
	}
	pExchange->Plan = pPlan;
	pExchange->Config = Config;
	pExchange->Events = Events;
	pExchange->OutputState = XRT_HTTP_EXCHANGE_OUTPUT_HEAD;
	pExchange->InputState = XRT_HTTP_EXCHANGE_INPUT_HEAD;
	pExchange->BodyRemaining =
		xrtHttp1RequestPlanBodyLength(pPlan);
	pExchange->ContinueAllowed =
		!xrtHttp1RequestPlanExpectContinue(pPlan);
	return pExchange;
}



/* 销毁 Exchange 的全部拥有型状态。 */
XRT_API void xrtHttp1ExchangeDestroy(
	xhttp1exchange* pExchange
)
{
	if ( pExchange == NULL ) {
		return;
	}
	__xrtHttp1ExchangeStopOutput(pExchange);
	xrtHttpBodyChunkRelease(&pExchange->Chunk);
	xrtHttpBodyReaderDestroy(pExchange->Reader);
	xrtFree(pExchange->Trailers);
	xrtFree(pExchange->Fields);
	xrtFree(pExchange->Pending.Data);
	xrtFree(pExchange->HeadBuffer.Data);
	xrtHttpResponseDestroy(pExchange->Response);
	xrtHttp1RequestPlanDestroy(pExchange->Plan);
	xrtErrorFree(pExchange->Error);
	memset(pExchange, 0, sizeof(*pExchange));
	xrtFree(pExchange);
}



/* 允许 100 Continue 正文路径继续。 */
XRT_API bool xrtHttp1ExchangeContinue(
	xhttp1exchange* pExchange
)
{
	if ( pExchange == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return false;
	}
	if ( pExchange->OutputStopped &&
		!pExchange->RequestComplete ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_STATE,
			XERR_STATE,
			"continue-http1-request",
			"HTTP/1 request output has already stopped"
		);
	}
	pExchange->ContinueAllowed = true;
	return true;
}



/* 暂停后续响应输入；当前回调借用的数据仍由本次 Feed 完成。 */
XRT_API bool xrtHttp1ExchangePause(xhttp1exchange* pExchange)
{
	if ( pExchange == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return false;
	}
	if ( pExchange->ResponseComplete || pExchange->Upgraded ) {
		__xrtHttp1ExchangeInvalidState();
		return false;
	}
	pExchange->Paused = true;
	return true;
}



/* 恢复后由下一次 Feed 继续消费调用方保留的输入。 */
XRT_API bool xrtHttp1ExchangeResume(xhttp1exchange* pExchange)
{
	if ( pExchange == NULL ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return false;
	}
	if ( pExchange->ResponseComplete || pExchange->Upgraded ) {
		__xrtHttp1ExchangeInvalidState();
		return false;
	}
	pExchange->Paused = false;
	return true;
}



/* 返回应用输入门的当前同步状态。 */
XRT_API bool xrtHttp1ExchangePaused(
	const xhttp1exchange* pExchange
)
{
	return (pExchange != NULL) && pExchange->Paused;
}



/* 返回最终响应的借用指针。 */
XRT_API const xhttpresponse* xrtHttp1ExchangeResponse(
	const xhttp1exchange* pExchange
)
{
	return pExchange != NULL ? pExchange->Response : NULL;
}



/* 在响应终态取走响应对象。 */
XRT_API xhttpresponse* xrtHttp1ExchangeTakeResponse(
	xhttp1exchange* pExchange
)
{
	xhttpresponse* pResponse;

	if ( (pExchange == NULL) ||
		!pExchange->ResponseComplete ||
		(pExchange->Response == NULL) ) {
		__xrtHttp1ExchangeInvalidState();
		return NULL;
	}
	pResponse = pExchange->Response;
	pExchange->Response = NULL;
	return pResponse;
}



/* 返回内部保留的终态后缀。 */
XRT_API xbytesview xrtHttp1ExchangeRemainder(
	const xhttp1exchange* pExchange
)
{
	if ( (pExchange == NULL) ||
		(!pExchange->ResponseComplete &&
		 (pExchange->Error == NULL)) ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){
		pExchange->Pending.Data,
		pExchange->Pending.Size
	};
}



/* 返回稳定 Exchange 错误。 */
XRT_API const xerror* xrtHttp1ExchangeError(
	const xhttp1exchange* pExchange
)
{
	return pExchange != NULL ? pExchange->Error : NULL;
}



/* 判断请求是否完整发送。 */
XRT_API bool xrtHttp1ExchangeRequestComplete(
	const xhttp1exchange* pExchange
)
{
	return (pExchange != NULL) &&
		pExchange->RequestComplete;
}



/* 判断响应是否完整结束。 */
XRT_API bool xrtHttp1ExchangeResponseComplete(
	const xhttp1exchange* pExchange
)
{
	return (pExchange != NULL) &&
		pExchange->ResponseComplete;
}



/* 判断 Exchange 是否交出升级协议。 */
XRT_API bool xrtHttp1ExchangeUpgraded(
	const xhttp1exchange* pExchange
)
{
	return (pExchange != NULL) && pExchange->Upgraded;
}



/* 汇总所有影响 HTTP/1 连接复用的协议事实。 */
XRT_API bool xrtHttp1ExchangeReusable(
	const xhttp1exchange* pExchange
)
{
	if ( (pExchange == NULL) ||
		(pExchange->Error != NULL) ||
		!pExchange->RequestComplete ||
		!pExchange->ResponseComplete ||
		pExchange->Upgraded ||
		pExchange->TransportEnded ||
		(pExchange->Pending.Size != 0) ||
		xrtHttp1RequestPlanClose(pExchange->Plan) ||
		(pExchange->BodyPlan.Mode == XHTTP1_BODY_CLOSE) ) {
		return false;
	}
	return (pExchange->ResponseFlags &
		(uint32)XHTTP1_KEEP_ALIVE) != 0;
}



/* 返回信息响应计数。 */
XRT_API uint32 xrtHttp1ExchangeInformationalCount(
	const xhttp1exchange* pExchange
)
{
	return pExchange != NULL ?
		pExchange->InformationalCount : 0;
}



/* 返回已经确认发送的线路字节。 */
XRT_API uint64 xrtHttp1ExchangeRequestWireBytes(
	const xhttp1exchange* pExchange
)
{
	return pExchange != NULL ?
		pExchange->RequestWireBytes : 0;
}

#endif
