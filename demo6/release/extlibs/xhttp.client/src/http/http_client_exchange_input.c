#include "../internal/xrt_http_exchange.h"




#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)

#define XRT_HTTP_EXCHANGE_INPUT_STEP 1024u



/* 返回输入中尚未处理的借用后缀，避免对空指针执行零偏移运算。 */
static xbytesview __xrtHttp1ExchangeInputAt(
	xbytesview Input,
	size_t iOffset
)
{
	if ( iOffset == Input.Size ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){
		Input.Data + iOffset,
		Input.Size - iOffset
	};
}



/* 推进 CRLFCRLF 的四状态匹配器。 */
static uint32 __xrtHttp1ExchangeDelimiter(
	uint32 iState,
	unsigned char iByte
)
{
	switch ( iState ) {
		case 0:
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		case 1:
			if ( iByte == (unsigned char)'\n' ) {
				return 2u;
			}
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		case 2:
			return iByte == (unsigned char)'\r' ? 3u : 0u;

		case 3:
			if ( iByte == (unsigned char)'\n' ) {
				return 4u;
			}
			return iByte == (unsigned char)'\r' ? 1u : 0u;

		default:
			return 4u;
	}
}



/* 返回 chunk 行或 trailer 跨输入边界时允许保留的最大字节数。 */
static size_t __xrtHttp1ExchangePendingLimit(
	const xhttp1exchange* pExchange
)
{
	size_t iChunk = pExchange->Config.Body.MaxChunkLine;

	if ( iChunk <= (SIZE_MAX - 2u) ) {
		iChunk += 2u;
	}
	return iChunk > pExchange->Config.Body.MaxTrailer ?
		iChunk : pExchange->Config.Body.MaxTrailer;
}



/* 下层错误存在时保留其类别，否则使用当前操作的保守类别。 */
static xerrkind __xrtHttp1ExchangeCauseKind(
	xerrkind Fallback
)
{
	const xerror* pCause = xrtGetError();

	return pCause != NULL ?
		xrtErrorKind(pCause) : Fallback;
}



/* 扩大借用字段描述符数组，字段文本仍由 Header 输入拥有。 */
static bool __xrtHttp1ExchangeFields(
	xhttp1exchange* pExchange,
	size_t iRequired
)
{
	xhttpfield* pFields;

	if ( iRequired <= pExchange->FieldCapacity ) {
		return true;
	}
	if ( iRequired > (SIZE_MAX / sizeof(xhttpfield)) ) {
		__xrtHttp1ExchangeSizeOverflow();
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			XERR_RANGE,
			"parse-http1-response",
			"HTTP/1 response field descriptor count overflowed"
		);
	}
	pFields = (xhttpfield*)xrtRealloc(
		pExchange->Fields,
		iRequired * sizeof(xhttpfield)
	);
	if ( pFields == NULL ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			XERR_MEMORY,
			"parse-http1-response",
			"HTTP/1 response field descriptor allocation failed",
			xrtGetError()
		);
	}
	pExchange->Fields = pFields;
	pExchange->FieldCapacity = iRequired;
	return true;
}



/* 扩大 trailer 描述符数组并原地重新绑定 Body Reader。 */
static bool __xrtHttp1ExchangeTrailers(
	xhttp1exchange* pExchange,
	size_t iRequired
)
{
	xhttpfield* pTrailers;

	if ( iRequired > pExchange->TrailerCapacity ) {
		if ( iRequired > (SIZE_MAX / sizeof(xhttpfield)) ) {
			__xrtHttp1ExchangeSizeOverflow();
			return __xrtHttp1ExchangeFail(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
				XERR_RANGE,
				"parse-http1-trailers",
				"HTTP/1 trailer descriptor count overflowed"
			);
		}
		pTrailers = (xhttpfield*)xrtRealloc(
			pExchange->Trailers,
			iRequired * sizeof(xhttpfield)
		);
		if ( pTrailers == NULL ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
				XERR_MEMORY,
				"parse-http1-trailers",
				"HTTP/1 trailer descriptor allocation failed",
				xrtGetError()
			);
		}
		pExchange->Trailers = pTrailers;
		pExchange->TrailerCapacity = iRequired;
	}
	if ( !xrtHttp1BodyTrailers(
		&pExchange->Body,
		pExchange->Trailers,
		pExchange->TrailerCapacity
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"parse-http1-trailers",
			"HTTP/1 trailer storage could not be rebound",
			xrtGetError()
		);
	}
	return true;
}



/* 把借用 Header 复制进一个只读拥有型响应。 */
static xhttpresponse* __xrtHttp1ExchangeResponseBuild(
	xhttp1exchange* pExchange,
	const xhttp1head* pHead
)
{
	xhttpresponse* pResponse;
	size_t i;

	pResponse = __xrtHttpResponseCreate(
		pHead->Version,
		pHead->Status,
		pHead->Reason,
		&pExchange->Config.Headers
	);
	if ( pResponse == NULL ) {
		(void)__xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			__xrtHttp1ExchangeCauseKind(XERR_MEMORY),
			"copy-http1-response",
			"HTTP/1 response object allocation failed",
			xrtGetError()
		);
		return NULL;
	}
	for ( i = 0; i < pHead->FieldCount; i++ ) {
		if ( !__xrtHttpResponseAddHeader(
			pResponse,
			pHead->Fields[i].Name,
			pHead->Fields[i].Value
		) ) {
			xrtHttpResponseDestroy(pResponse);
			(void)__xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
				__xrtHttp1ExchangeCauseKind(XERR_MEMORY),
				"copy-http1-response",
				"HTTP/1 response Header could not be stored",
				xrtGetError()
			);
			return NULL;
		}
	}
	if ( !__xrtHttpResponseSetUrl(
		pResponse,
		xrtHttp1RequestPlanUrl(pExchange->Plan)
	) ) {
		xrtHttpResponseDestroy(pResponse);
		(void)__xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			__xrtHttp1ExchangeCauseKind(XERR_MEMORY),
			"copy-http1-response",
			"HTTP/1 response URL could not be stored",
			xrtGetError()
		);
		return NULL;
	}
	return pResponse;
}



/* 判断请求方法是否为大小写敏感的 CONNECT。 */
static bool __xrtHttp1ExchangeConnect(
	const xhttp1exchange* pExchange
)
{
	xstrview Method = xrtHttp1RequestPlanMethod(
		pExchange->Plan
	);

	return xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("CONNECT")
	);
}



/* 拒绝 RFC 明确禁止或会形成歧义的响应分帧组合。 */
static bool __xrtHttp1ExchangeFramingValid(
	xhttp1exchange* pExchange,
	const xhttp1head* pHead
)
{
	uint32 iFraming = pHead->Flags &
		((uint32)XHTTP1_CONTENT_LENGTH |
		 (uint32)XHTTP1_TRANSFER_ENCODING);

	if ( __xrtHttp1ExchangeConnect(pExchange) &&
		(pHead->Status >= 200) &&
		(pHead->Status < 300) ) {
		return true;
	}
	if ( iFraming ==
		((uint32)XHTTP1_CONTENT_LENGTH |
		 (uint32)XHTTP1_TRANSFER_ENCODING) ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"frame-http1-response",
			"HTTP/1 response combines Transfer-Encoding and Content-Length"
		);
	}
	if ( ((pHead->Flags & (uint32)XHTTP1_TRANSFER_OTHER) != 0) &&
		!pExchange->Config.AllowRawTransferCodings ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_UNSUPPORTED,
			"frame-http1-response",
			"HTTP/1 response uses an unsupported Transfer-Encoding"
		);
	}
	if ( ((pHead->Status < 200) ||
		 (pHead->Status == 204)) &&
		(iFraming != 0) ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"frame-http1-response",
			"HTTP/1 response contains forbidden framing metadata"
		);
	}
	return true;
}



/* 复制并发布一个有界信息响应。 */
static bool __xrtHttp1ExchangeInformational(
	xhttp1exchange* pExchange,
	const xhttp1head* pHead
)
{
	xhttpresponse* pResponse = NULL;
	bool bAccepted = true;

	pExchange->InformationalCount++;
	if ( pExchange->InformationalCount >
		pExchange->Config.MaxInformational ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_INFORMATIONAL_LIMIT,
			XERR_RANGE,
			"receive-http1-informational",
			"HTTP/1 informational response count exceeds its limit"
		);
	}
	if ( pHead->Status == 100 ) {
		pExchange->ContinueAllowed = true;
	}
	if ( pExchange->Events.Informational != NULL ) {
		pResponse = __xrtHttp1ExchangeResponseBuild(
			pExchange, pHead
		);
		if ( pResponse == NULL ) {
			return false;
		}
		/* 只允许本次回调产生的错误进入 Exchange 原因链。 */
		xrtClearError();
		bAccepted = pExchange->Events.Informational(
			pResponse,
			pExchange->Events.Data
		);
		xrtHttpResponseDestroy(pResponse);
	}
	if ( !bAccepted ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK,
			XERR_CANCELLED,
			"deliver-http1-informational",
			"HTTP/1 informational response callback stopped the exchange",
			xrtGetError()
		);
	}
	return true;
}



/* 把最终响应 Header 转换为唯一 Body Plan 并发布 Header 回调。 */
static bool __xrtHttp1ExchangeFinalHead(
	xhttp1exchange* pExchange,
	const xhttp1head* pHead
)
{
	xhttpresponse* pResponse;

	if ( !xrtHttp1ResponseBodyPlan(
		pHead,
		xrtHttp1RequestPlanMethod(pExchange->Plan),
		&pExchange->BodyPlan
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"frame-http1-response",
			"HTTP/1 response body framing is invalid",
			xrtGetError()
		);
	}
	pResponse = __xrtHttp1ExchangeResponseBuild(
		pExchange, pHead
	);
	if ( pResponse == NULL ) {
		return false;
	}
	if ( pExchange->Events.Body != NULL ) {
		__xrtHttpResponseSetFlags(
			pResponse,
			(uint32)XHTTP_RESPONSE_STREAMED
		);
	}
	pExchange->Response = pResponse;
	pExchange->ResponseFlags = pHead->Flags;
	if ( !pExchange->RequestComplete ) {
		__xrtHttp1ExchangeStopOutput(pExchange);
	}
	if ( pExchange->Events.Headers != NULL ) {
		bool bAccepted;

		/* 避免把 Feed 前遗留的无关错误误挂到回调失败上。 */
		xrtClearError();
		bAccepted = pExchange->Events.Headers(
			pResponse,
			pExchange->Events.Data
		);
		if ( !bAccepted ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK,
				XERR_CANCELLED,
				"deliver-http1-response-head",
				"HTTP/1 response Header callback stopped the exchange",
				xrtGetError()
			);
		}
	}
	if ( pExchange->BodyPlan.Mode == XHTTP1_BODY_TUNNEL ) {
		__xrtHttpResponseSetFlags(
			pResponse,
			(uint32)XHTTP_RESPONSE_UPGRADED
		);
		pExchange->Upgraded = true;
		pExchange->ResponseComplete = true;
		pExchange->InputState =
			XRT_HTTP_EXCHANGE_INPUT_UPGRADED;
		return true;
	}
	if ( !xrtHttp1BodyInit(
		&pExchange->Body,
		&pExchange->BodyPlan,
		NULL,
		0,
		&pExchange->Config.Body
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			__xrtHttp1ExchangeCauseKind(XERR_PROTOCOL),
			"init-http1-response-body",
			"HTTP/1 response Body Reader could not be initialized",
			xrtGetError()
		);
	}
	pExchange->InputState = XRT_HTTP_EXCHANGE_INPUT_BODY;
	return true;
}



/* 解析已经完整收集的一条响应 Header。 */
static bool __xrtHttp1ExchangeHeadParse(
	xhttp1exchange* pExchange
)
{
	xhttp1errorinfo Error;
	xhttp1head Head;
	xhttp1status Status;
	size_t iTrailerNames;
	xbytesview Input = {
		pExchange->HeadBuffer.Data,
		pExchange->HeadBuffer.Size
	};

	xrtHttp1HeadInit(
		&Head,
		pExchange->Fields,
		pExchange->FieldCapacity
	);
	Status = xrtHttp1ResponseParse(
		Input,
		&Head,
		&pExchange->Config.Head,
		&Error
	);
	if ( Status == XHTTP1_FIELDS ) {
		if ( !__xrtHttp1ExchangeFields(
			pExchange, Head.FieldCount
		) ) {
			return false;
		}
		xrtHttp1HeadInit(
			&Head,
			pExchange->Fields,
			pExchange->FieldCapacity
		);
		Status = xrtHttp1ResponseParse(
			Input,
			&Head,
			&pExchange->Config.Head,
			&Error
		);
	}
	if ( Status != XHTTP1_READY ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			XERR_PROTOCOL,
			"parse-http1-response",
			"HTTP/1 response Header is invalid",
			xrtGetError()
		);
	}
	if ( !__xrtHttp1ExchangeFramingValid(
		pExchange, &Head
	) ) {
		return false;
	}
	if ( !xrtHttpTrailerCount(
		Head.Fields, Head.FieldCount, &iTrailerNames
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
			XERR_PROTOCOL,
			"validate-http1-response-trailer",
			"HTTP/1 response contains an invalid Trailer declaration",
			xrtGetError()
		);
	}
	if ( (Head.Status >= 100) &&
		(Head.Status < 200) &&
		(Head.Status != 101) ) {
		return __xrtHttp1ExchangeInformational(
			pExchange, &Head
		);
	}
	return __xrtHttp1ExchangeFinalHead(
		pExchange, &Head
	);
}



/* 收集到第一个 CRLFCRLF 为止，不吞入正文或下一协议字节。 */
static bool __xrtHttp1ExchangeHeadFeed(
	xhttp1exchange* pExchange,
	xbytesview Input,
	size_t* pTaken,
	bool* pReady
)
{
	size_t i;

	*pTaken = 0;
	*pReady = false;
	for ( i = 0; i < Input.Size; i++ ) {
		pExchange->HeadDelimiter =
			__xrtHttp1ExchangeDelimiter(
				pExchange->HeadDelimiter,
				Input.Data[i]
			);
		if ( pExchange->HeadDelimiter == 4 ) {
			i++;
			break;
		}
	}
	if ( i != 0 ) {
		if ( !__xrtHttp1ExchangeBufferAppend(
			pExchange,
			&pExchange->HeadBuffer,
			(xbytesview){ Input.Data, i },
			pExchange->Config.Head.MaxHead
		) ) {
			return false;
		}
	}
	*pTaken = i;
	if ( pExchange->HeadDelimiter == 4 ) {
		*pReady = true;
		return true;
	}
	if ( pExchange->HeadBuffer.Size >=
		pExchange->Config.Head.MaxHead ) {
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
			XERR_RANGE,
			"receive-http1-response-head",
			"HTTP/1 response Header exceeds its limit"
		);
	}
	return true;
}



/* 完成响应，复制经过严格验证的 trailer。 */
static bool __xrtHttp1ExchangeResponseFinish(
	xhttp1exchange* pExchange
)
{
	size_t i;

	if ( !xrtHttpTrailerSectionValid(
		pExchange->Body.Trailers,
		pExchange->Body.TrailerCount
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"validate-http1-response-trailers",
			"HTTP/1 response contains an invalid trailer section",
			xrtGetError()
		);
	}
	for ( i = 0; i < pExchange->Body.TrailerCount; i++ ) {
		if ( !__xrtHttpResponseAddTrailer(
			pExchange->Response,
			&pExchange->Config.Trailers,
			pExchange->Body.Trailers[i].Name,
			pExchange->Body.Trailers[i].Value
		) ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
				__xrtHttp1ExchangeCauseKind(XERR_MEMORY),
				"copy-http1-trailers",
				"HTTP/1 response trailer could not be stored",
				xrtGetError()
			);
		}
	}
	pExchange->ResponseComplete = true;
	pExchange->InputState = XRT_HTTP_EXCHANGE_INPUT_DONE;
	return true;
}



/* 同步交付一个解帧后的正文片段。 */
static bool __xrtHttp1ExchangeBodyDeliver(
	xhttp1exchange* pExchange,
	xbytesview Data
)
{
	if ( !__xrtHttpResponseAddWireBody(
		pExchange->Response,
		(uint64)Data.Size
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_RANGE,
			"count-http1-response-body",
			"HTTP/1 response body byte count overflowed",
			xrtGetError()
		);
	}
	if ( pExchange->Events.Body != NULL ) {
		bool bAccepted;

		/* Body 消费器的分配或持久化错误必须穿透到高层调用。 */
		xrtClearError();
		bAccepted = pExchange->Events.Body(
			pExchange->Response,
			Data,
			pExchange->Events.Data
		);
		if ( !bAccepted ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK,
				XERR_CANCELLED,
				"deliver-http1-response-body",
				"HTTP/1 response body callback stopped the exchange",
				xrtGetError()
			);
		}
		if ( !__xrtHttpResponseDeliverBody(
			pExchange->Response,
			(uint64)Data.Size
		) ) {
			return __xrtHttp1ExchangeFailCause(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
				XERR_RANGE,
				"count-http1-response-body",
				"HTTP/1 streamed response body byte count overflowed",
				xrtGetError()
			);
		}
		return true;
	}
	if ( !__xrtHttpResponseAppendBody(
		pExchange->Response, Data
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			__xrtHttp1ExchangeCauseKind(XERR_MEMORY),
			"buffer-http1-response-body",
			"HTTP/1 response body could not be buffered",
			xrtGetError()
		);
	}
	return true;
}



/*
	推进 Body Reader。
	返回值通过 Done 区分终态，Consumed 始终只覆盖当前输入前缀。
*/
static bool __xrtHttp1ExchangeBodyFeed(
	xhttp1exchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	bool* pDone,
	bool* pRetry
)
{
	xhttp1errorinfo Error;
	xhttp1bodystatus Status;
	xbytesview Data;

	*pConsumed = 0;
	*pDone = false;
	*pRetry = false;
	Status = xrtHttp1BodyRead(
		&pExchange->Body,
		Input,
		bEnd,
		pConsumed,
		&Data,
		&Error
	);
	if ( Status == XHTTP1_BODY_DATA ) {
		return __xrtHttp1ExchangeBodyDeliver(
			pExchange, Data
		);
	}
	if ( Status == XHTTP1_BODY_FIELDS ) {
		if ( !__xrtHttp1ExchangeTrailers(
			pExchange,
			pExchange->Body.TrailerCount
		) ) {
			return false;
		}
		*pRetry = true;
		return true;
	}
	if ( Status == XHTTP1_BODY_DONE ) {
		if ( !__xrtHttp1ExchangeResponseFinish(
			pExchange
		) ) {
			return false;
		}
		*pDone = true;
		return true;
	}
	if ( Status == XHTTP1_BODY_ERROR ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
			XERR_PROTOCOL,
			"parse-http1-response-body",
			"HTTP/1 response body framing is invalid",
			xrtGetError()
		);
	}
	return true;
}



/* 把入站终态转换为公开 Feed 状态。 */
static xhttp1feedstatus __xrtHttp1ExchangeFeedState(
	xhttp1exchange* pExchange,
	bool bEnd
)
{
	if ( bEnd ) {
		pExchange->TransportEnded = true;
	}
	if ( pExchange->Error != NULL ) {
		return XHTTP1_FEED_ERROR;
	}
	if ( pExchange->Upgraded ) {
		return XHTTP1_FEED_UPGRADED;
	}
	if ( pExchange->ResponseComplete ) {
		return XHTTP1_FEED_DONE;
	}
	if ( pExchange->Paused ) {
		return XHTTP1_FEED_PAUSED;
	}
	return XHTTP1_FEED_MORE;
}



/* 增量消费一条 HTTP/1 响应。 */
XRT_API xhttp1feedstatus xrtHttp1ExchangeFeed(
	xhttp1exchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
)
{
	size_t iAccepted = 0;
	size_t iConsumed;
	bool bReady;
	bool bDone;
	bool bRetry;

	if ( pAccepted != NULL ) {
		*pAccepted = 0;
	}
	if ( (pExchange == NULL) || (pAccepted == NULL) ||
		((Input.Data == NULL) && (Input.Size != 0)) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return XHTTP1_FEED_ERROR;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return XHTTP1_FEED_ERROR;
	}
	if ( (pExchange->InputState ==
		 XRT_HTTP_EXCHANGE_INPUT_DONE) ||
		(pExchange->InputState ==
		 XRT_HTTP_EXCHANGE_INPUT_UPGRADED) ) {
		(void)__xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_STATE,
			XERR_STATE,
			"feed-http1-response",
			"HTTP/1 Exchange response is already complete"
		);
		return XHTTP1_FEED_ERROR;
	}
	if ( pExchange->Paused ) {
		if ( bEnd ) {
			pExchange->TransportEnded = true;
		}
		return XHTTP1_FEED_PAUSED;
	}
	if ( pExchange->TransportEnded ) {
		bEnd = true;
	}
	for ( ;; ) {
		if ( pExchange->Paused ) {
			if ( bEnd ) {
				pExchange->TransportEnded = true;
			}
			*pAccepted = iAccepted;
			return XHTTP1_FEED_PAUSED;
		}
		if ( pExchange->InputState ==
			XRT_HTTP_EXCHANGE_INPUT_HEAD ) {
			if ( !__xrtHttp1ExchangeHeadFeed(
				pExchange,
				__xrtHttp1ExchangeInputAt(
					Input, iAccepted
				),
				&iConsumed,
				&bReady
			) ) {
				*pAccepted = iAccepted;
				return XHTTP1_FEED_ERROR;
			}
			iAccepted += iConsumed;
			if ( !bReady ) {
				*pAccepted = iAccepted;
				if ( bEnd && (iAccepted == Input.Size) ) {
					(void)__xrtHttp1ExchangeFail(
						pExchange,
						XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF,
						XERR_PROTOCOL,
						"receive-http1-response-head",
						"HTTP/1 response ended before a complete Header"
					);
					return XHTTP1_FEED_ERROR;
				}
				return XHTTP1_FEED_MORE;
			}
			if ( !__xrtHttp1ExchangeHeadParse(
				pExchange
			) ) {
				*pAccepted = iAccepted;
				return XHTTP1_FEED_ERROR;
			}
			pExchange->HeadBuffer.Size = 0;
			pExchange->HeadDelimiter = 0;
			if ( pExchange->InputState ==
				XRT_HTTP_EXCHANGE_INPUT_HEAD ) {
				if ( iAccepted == Input.Size ) {
					*pAccepted = iAccepted;
					if ( bEnd ) {
						(void)__xrtHttp1ExchangeFail(
							pExchange,
							XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF,
							XERR_PROTOCOL,
							"receive-http1-response-head",
							"HTTP/1 connection ended after an informational response"
						);
						return XHTTP1_FEED_ERROR;
					}
					return XHTTP1_FEED_MORE;
				}
				continue;
			}
			if ( pExchange->ResponseComplete ) {
				*pAccepted = iAccepted;
				return __xrtHttp1ExchangeFeedState(
					pExchange, bEnd
				);
			}
		}

		if ( pExchange->InputState !=
			XRT_HTTP_EXCHANGE_INPUT_BODY ) {
			*pAccepted = iAccepted;
			return __xrtHttp1ExchangeFeedState(
				pExchange, bEnd
			);
		}

		if ( pExchange->Pending.Size != 0 ) {
			if ( !__xrtHttp1ExchangeBodyFeed(
				pExchange,
				(xbytesview){
					pExchange->Pending.Data,
					pExchange->Pending.Size
				},
				bEnd && (iAccepted == Input.Size),
				&iConsumed,
				&bDone,
				&bRetry
			) ) {
				*pAccepted = iAccepted;
				return XHTTP1_FEED_ERROR;
			}
			__xrtHttp1ExchangeBufferConsume(
				&pExchange->Pending, iConsumed
			);
			if ( bDone ) {
				*pAccepted = iAccepted;
				return __xrtHttp1ExchangeFeedState(
					pExchange, bEnd
				);
			}
			if ( bRetry ) {
				continue;
			}
			if ( iConsumed != 0 ) {
				continue;
			}
			if ( iAccepted < Input.Size ) {
				size_t iLimit =
					__xrtHttp1ExchangePendingLimit(
						pExchange
					);
				size_t iRoom = iLimit -
					pExchange->Pending.Size;
				size_t iStep = Input.Size - iAccepted;

				if ( iStep > XRT_HTTP_EXCHANGE_INPUT_STEP ) {
					iStep = XRT_HTTP_EXCHANGE_INPUT_STEP;
				}
				if ( iStep > iRoom ) {
					iStep = iRoom;
				}
				if ( (iStep == 0) ||
					!__xrtHttp1ExchangeBufferAppend(
						pExchange,
						&pExchange->Pending,
						(xbytesview){
							Input.Data + iAccepted,
							iStep
						},
						iLimit
					) ) {
					if ( pExchange->Error == NULL ) {
						(void)__xrtHttp1ExchangeFail(
							pExchange,
							XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
							XERR_RANGE,
							"buffer-http1-response-body",
							"HTTP/1 partial body framing exceeds its limit"
						);
					}
					*pAccepted = iAccepted;
					return XHTTP1_FEED_ERROR;
				}
				iAccepted += iStep;
				continue;
			}
			*pAccepted = iAccepted;
			if ( bEnd ) {
				(void)__xrtHttp1ExchangeFail(
					pExchange,
					XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF,
					XERR_PROTOCOL,
					"receive-http1-response-body",
					"HTTP/1 response ended before its body framing completed"
				);
				return XHTTP1_FEED_ERROR;
			}
			return XHTTP1_FEED_MORE;
		}

		bReady = __xrtHttp1ExchangeBodyFeed(
			pExchange,
			__xrtHttp1ExchangeInputAt(
				Input, iAccepted
			),
			bEnd,
			&iConsumed,
			&bDone,
			&bRetry
		);
		iAccepted += iConsumed;
		if ( !bReady ) {
			*pAccepted = iAccepted;
			return XHTTP1_FEED_ERROR;
		}
		if ( bDone ) {
			*pAccepted = iAccepted;
			return __xrtHttp1ExchangeFeedState(
				pExchange, bEnd
			);
		}
		if ( bRetry ) {
			continue;
		}
		if ( iConsumed != 0 ) {
			continue;
		}
		if ( iAccepted < Input.Size ) {
			size_t iStep = Input.Size - iAccepted;
			size_t iLimit =
				__xrtHttp1ExchangePendingLimit(
					pExchange
				);

			if ( iStep > XRT_HTTP_EXCHANGE_INPUT_STEP ) {
				iStep = XRT_HTTP_EXCHANGE_INPUT_STEP;
			}
			if ( !__xrtHttp1ExchangeBufferAppend(
				pExchange,
				&pExchange->Pending,
				(xbytesview){
					Input.Data + iAccepted,
					iStep
				},
				iLimit
			) ) {
				*pAccepted = iAccepted;
				return XHTTP1_FEED_ERROR;
			}
			iAccepted += iStep;
			continue;
		}
		*pAccepted = iAccepted;
		if ( bEnd ) {
			(void)__xrtHttp1ExchangeFail(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF,
				XERR_PROTOCOL,
				"receive-http1-response-body",
				"HTTP/1 response ended before its body completed"
			);
			return XHTTP1_FEED_ERROR;
		}
		return XHTTP1_FEED_MORE;
	}
}

#endif

