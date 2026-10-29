#include "../internal/xrt_http_exchange.h"



#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)

/* 发布一个借用出站片段并记录本次允许消费的长度。 */
static xhttp1outputstatus __xrtHttp1ExchangeOffer(
	xhttp1exchange* pExchange,
	cbytes pData,
	size_t iSize,
	size_t iMaxBytes,
	xbytesview* pOutput
)
{
	size_t iOffer = iSize < iMaxBytes ?
		iSize : iMaxBytes;

	pExchange->OfferData = pData;
	pExchange->Offered = iOffer;
	*pOutput = (xbytesview){ pData, iOffer };
	return XHTTP1_OUTPUT_DATA;
}



/* 标记请求线路已经完整发送并关闭正文 Reader。 */
static xhttp1outputstatus __xrtHttp1ExchangeOutputFinish(
	xhttp1exchange* pExchange
)
{
	xrtHttpBodyChunkRelease(&pExchange->Chunk);
	xrtHttpBodyReaderDestroy(pExchange->Reader);
	pExchange->Reader = NULL;
	pExchange->OutputState = XRT_HTTP_EXCHANGE_OUTPUT_DONE;
	pExchange->OutputStopped = true;
	pExchange->RequestComplete = true;
	pExchange->OutputAgain = false;
	return XHTTP1_OUTPUT_DONE;
}



/* 建立正文 Reader；无正文对象的显式 chunked 请求只发送 last-chunk。 */
static bool __xrtHttp1ExchangeOutputOpen(
	xhttp1exchange* pExchange
)
{
	xhttpbody* pBody;

	if ( pExchange->ReaderOpened ) {
		return true;
	}
	pExchange->ReaderOpened = true;
	pBody = xrtHttp1RequestPlanBody(pExchange->Plan);
	if ( pBody == NULL ) {
		return true;
	}
	pExchange->Reader = xrtHttpBodyOpen(pBody);
	if ( pExchange->Reader == NULL ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_REQUEST_BODY,
			XERR_IO,
			"open-http-request-body",
			"HTTP request body could not be opened",
			xrtGetError()
		);
	}
	return true;
}



/* 准备 chunk-size 行。 */
static bool __xrtHttp1ExchangeChunkLine(
	xhttp1exchange* pExchange
)
{
	if ( !xrtHttp1ChunkLineWrite(
		(uint64)pExchange->Chunk.Size,
		(xstrview){ NULL, 0 },
		pExchange->ChunkLine,
		sizeof(pExchange->ChunkLine),
		&pExchange->ChunkLineSize
	) ) {
		return __xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_REQUEST_BODY,
			XERR_PROTOCOL,
			"frame-http-request-body",
			"HTTP request body chunk could not be framed",
			xrtGetError()
		);
	}
	pExchange->PartOffset = 0;
	pExchange->ChunkTerminal = false;
	pExchange->OutputState =
		XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_LINE;
	return true;
}



/* 切换到请求计划已经冻结的完整 last-chunk。 */
static void __xrtHttp1ExchangeChunkEnd(
	xhttp1exchange* pExchange
)
{
	pExchange->PartOffset = 0;
	pExchange->ChunkTerminal = true;
	pExchange->OutputState =
		XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_END;
}



/* 从正文源取得下一个拥有型片段并选择固定或 chunked 线路状态。 */
static xhttp1outputstatus __xrtHttp1ExchangeBodyNext(
	xhttp1exchange* pExchange,
	size_t iMaxBytes
)
{
	xhttprequestbodymode Mode =
		xrtHttp1RequestPlanBodyMode(pExchange->Plan);
	xhttpbodystatus Status;
	size_t iReadMax = iMaxBytes;

	if ( !__xrtHttp1ExchangeOutputOpen(pExchange) ) {
		return XHTTP1_OUTPUT_ERROR;
	}
	if ( pExchange->Reader == NULL ) {
		if ( Mode == XHTTP_REQUEST_BODY_CHUNKED ) {
			__xrtHttp1ExchangeChunkEnd(pExchange);
			return XHTTP1_OUTPUT_DATA;
		}
		if ( pExchange->BodyRemaining != 0 ) {
			(void)__xrtHttp1ExchangeFail(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH,
				XERR_PROTOCOL,
				"read-http-request-body",
				"HTTP request body ended before its declared length"
			);
			return XHTTP1_OUTPUT_ERROR;
		}
		return __xrtHttp1ExchangeOutputFinish(pExchange);
	}
	if ( (Mode == XHTTP_REQUEST_BODY_FIXED) &&
		(pExchange->BodyRemaining < (uint64)iReadMax) ) {
		iReadMax = (size_t)pExchange->BodyRemaining;
	}
	if ( iReadMax == 0 ) {
		iReadMax = 1;
	}
	Status = xrtHttpBodyNext(
		pExchange->Reader,
		iReadMax,
		&pExchange->Chunk
	);
	if ( Status == XHTTP_BODY_AGAIN ) {
		pExchange->OutputAgain = true;
		return XHTTP1_OUTPUT_AGAIN;
	}
	pExchange->OutputAgain = false;
	if ( Status == XHTTP_BODY_ERROR ) {
		(void)__xrtHttp1ExchangeFailCause(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_REQUEST_BODY,
			XERR_IO,
			"read-http-request-body",
			"HTTP request body source failed",
			xrtHttpBodyReaderError(pExchange->Reader)
		);
		return XHTTP1_OUTPUT_ERROR;
	}
	if ( Status == XHTTP_BODY_EOF ) {
		if ( (Mode == XHTTP_REQUEST_BODY_FIXED) &&
			(pExchange->BodyRemaining != 0) ) {
			(void)__xrtHttp1ExchangeFail(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH,
				XERR_PROTOCOL,
				"read-http-request-body",
				"HTTP request body ended before its declared length"
			);
			return XHTTP1_OUTPUT_ERROR;
		}
		if ( Mode == XHTTP_REQUEST_BODY_CHUNKED ) {
			__xrtHttp1ExchangeChunkEnd(pExchange);
			return XHTTP1_OUTPUT_DATA;
		}
		return __xrtHttp1ExchangeOutputFinish(pExchange);
	}
	if ( (Mode == XHTTP_REQUEST_BODY_FIXED) &&
		((uint64)pExchange->Chunk.Size >
		 pExchange->BodyRemaining) ) {
		(void)__xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH,
			XERR_PROTOCOL,
			"read-http-request-body",
			"HTTP request body exceeded its declared length"
		);
		return XHTTP1_OUTPUT_ERROR;
	}
	pExchange->PartOffset = 0;
	if ( Mode == XHTTP_REQUEST_BODY_CHUNKED ) {
		if ( !__xrtHttp1ExchangeChunkLine(pExchange) ) {
			return XHTTP1_OUTPUT_ERROR;
		}
	} else {
		pExchange->OutputState =
			XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_DATA;
	}
	return XHTTP1_OUTPUT_DATA;
}



/* 借出下一段 HTTP/1 请求线路数据。 */
XRT_API xhttp1outputstatus xrtHttp1ExchangeOutput(
	xhttp1exchange* pExchange,
	size_t iMaxBytes,
	xbytesview* pData
)
{
	xbytesview Head;
	xbytesview End;
	xhttp1outputstatus Status;

	if ( pData != NULL ) {
		*pData = (xbytesview){ NULL, 0 };
	}
	if ( (pExchange == NULL) || (pData == NULL) ||
		(iMaxBytes == 0) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return XHTTP1_OUTPUT_ERROR;
	}
	if ( pExchange->Error != NULL ) {
		xrtSetError(pExchange->Error);
		return XHTTP1_OUTPUT_ERROR;
	}
	if ( pExchange->OutputStopped ) {
		return XHTTP1_OUTPUT_DONE;
	}
	if ( pExchange->Offered != 0 ) {
		*pData = (xbytesview){
			pExchange->OfferData,
			pExchange->Offered
		};
		return XHTTP1_OUTPUT_DATA;
	}
	for ( ;; ) {
		switch ( pExchange->OutputState ) {
			case XRT_HTTP_EXCHANGE_OUTPUT_HEAD:
				Head = xrtHttp1RequestPlanHead(
					pExchange->Plan
				);
				if ( pExchange->HeadOffset < Head.Size ) {
					return __xrtHttp1ExchangeOffer(
						pExchange,
						Head.Data + pExchange->HeadOffset,
						Head.Size - pExchange->HeadOffset,
						iMaxBytes,
						pData
					);
				}
				pExchange->OutputState =
					XRT_HTTP_EXCHANGE_OUTPUT_BODY;
				break;

			case XRT_HTTP_EXCHANGE_OUTPUT_BODY:
				if ( xrtHttp1RequestPlanBodyMode(
					pExchange->Plan
				) == XHTTP_REQUEST_BODY_NONE ) {
					return __xrtHttp1ExchangeOutputFinish(
						pExchange
					);
				}
				if ( !pExchange->ContinueAllowed ) {
					return XHTTP1_OUTPUT_CONTINUE;
				}
				Status = __xrtHttp1ExchangeBodyNext(
					pExchange, iMaxBytes
				);
				if ( Status != XHTTP1_OUTPUT_DATA ) {
					return Status;
				}
				break;

			case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_LINE:
				return __xrtHttp1ExchangeOffer(
					pExchange,
					(cbytes)pExchange->ChunkLine +
						pExchange->PartOffset,
					pExchange->ChunkLineSize -
						pExchange->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_DATA:
				return __xrtHttp1ExchangeOffer(
					pExchange,
					pExchange->Chunk.Data +
						pExchange->PartOffset,
					pExchange->Chunk.Size -
						pExchange->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_END:
				End = pExchange->ChunkTerminal ?
					xrtHttp1RequestPlanEnd(
						pExchange->Plan
					) :
					(xbytesview){
						(cbytes)pExchange->ChunkLine,
						pExchange->ChunkLineSize
					};
				if ( pExchange->PartOffset >= End.Size ) {
					(void)__xrtHttp1ExchangeFail(
						pExchange,
						XHTTP1_EXCHANGE_ERROR_STATE,
						XERR_INTERNAL,
						"output-http1-request",
						"HTTP/1 request chunk terminator is invalid"
					);
					return XHTTP1_OUTPUT_ERROR;
				}
				return __xrtHttp1ExchangeOffer(
					pExchange,
					End.Data +
						pExchange->PartOffset,
					End.Size -
						pExchange->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_EXCHANGE_OUTPUT_DONE:
				return XHTTP1_OUTPUT_DONE;

			default:
				(void)__xrtHttp1ExchangeFail(
					pExchange,
					XHTTP1_EXCHANGE_ERROR_STATE,
					XERR_INTERNAL,
					"output-http1-request",
					"HTTP/1 request output state is invalid"
				);
				return XHTTP1_OUTPUT_ERROR;
		}
	}
}



/* 推进最近一次借出的请求线路片段。 */
XRT_API bool xrtHttp1ExchangeOutputConsume(
	xhttp1exchange* pExchange,
	size_t iSize
)
{
	xhttprequestbodymode Mode;
	size_t iPartSize;

	if ( (pExchange == NULL) ||
		(pExchange->Offered == 0) ||
		(iSize > pExchange->Offered) ) {
		__xrtHttp1ExchangeInvalidArgument();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pExchange->RequestWireBytes >
		(UINT64_MAX - (uint64)iSize) ) {
		__xrtHttp1ExchangeSizeOverflow();
		return __xrtHttp1ExchangeFail(
			pExchange,
			XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH,
			XERR_RANGE,
			"consume-http1-request",
			"HTTP/1 request wire byte count overflowed"
		);
	}
	pExchange->RequestWireBytes += (uint64)iSize;
	pExchange->Offered = 0;
	pExchange->OfferData = NULL;
	if ( pExchange->OutputStopped ) {
		xrtHttpBodyChunkRelease(&pExchange->Chunk);
		xrtHttpBodyReaderDestroy(pExchange->Reader);
		pExchange->Reader = NULL;
		pExchange->OutputAgain = false;
		return true;
	}
	switch ( pExchange->OutputState ) {
		case XRT_HTTP_EXCHANGE_OUTPUT_HEAD:
			pExchange->HeadOffset += iSize;
			break;

		case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_LINE:
			pExchange->PartOffset += iSize;
			if ( pExchange->PartOffset ==
				pExchange->ChunkLineSize ) {
				pExchange->PartOffset = 0;
				pExchange->OutputState =
					XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_DATA;
			}
			break;

		case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_DATA:
			Mode = xrtHttp1RequestPlanBodyMode(
				pExchange->Plan
			);
			pExchange->PartOffset += iSize;
			if ( Mode == XHTTP_REQUEST_BODY_FIXED ) {
				pExchange->BodyRemaining -= (uint64)iSize;
			}
			if ( pExchange->PartOffset ==
				pExchange->Chunk.Size ) {
				xrtHttpBodyChunkRelease(
					&pExchange->Chunk
				);
				pExchange->PartOffset = 0;
				if ( Mode ==
					XHTTP_REQUEST_BODY_CHUNKED ) {
					memcpy(
						pExchange->ChunkLine,
						"\r\n",
						2
					);
					pExchange->ChunkLineSize = 2;
					pExchange->ChunkTerminal = false;
					pExchange->OutputState =
						XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_END;
				} else {
					pExchange->OutputState =
						XRT_HTTP_EXCHANGE_OUTPUT_BODY;
				}
			}
			break;

		case XRT_HTTP_EXCHANGE_OUTPUT_CHUNK_END:
			iPartSize = pExchange->ChunkTerminal ?
				xrtHttp1RequestPlanEnd(
					pExchange->Plan
				).Size : pExchange->ChunkLineSize;
			pExchange->PartOffset += iSize;
			if ( pExchange->PartOffset ==
				iPartSize ) {
				pExchange->PartOffset = 0;
				if ( pExchange->ChunkTerminal ) {
					(void)__xrtHttp1ExchangeOutputFinish(
						pExchange
					);
				} else {
					pExchange->OutputState =
						XRT_HTTP_EXCHANGE_OUTPUT_BODY;
				}
			}
			break;

		default:
			return __xrtHttp1ExchangeFail(
				pExchange,
				XHTTP1_EXCHANGE_ERROR_STATE,
				XERR_INTERNAL,
				"consume-http1-request",
				"HTTP/1 request output cannot be consumed in this state"
			);
	}
	return true;
}

#endif

