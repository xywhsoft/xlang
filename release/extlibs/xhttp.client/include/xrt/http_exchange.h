#ifndef XRT_HTTP_EXCHANGE_H
#define XRT_HTTP_EXCHANGE_H




#if defined(XHTTP_FEATURE_HTTP_EXCHANGE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE) || \
	 !defined(XRT_FEATURE_HTTP1_BODY) || \
	 !defined(XRT_FEATURE_HTTP_TRAILER))
	#error "XRT HTTP exchange support requires request prepare, response, HTTP/1 body and Trailer support"
#endif



#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)

/* Exchange 拥有一个请求计划并独立推进出站与入站状态机。 */
typedef struct xhttp1exchange xhttp1exchange;



/* 出站状态区分正文源暂不可读与 100 Continue 策略等待。 */
typedef enum xhttp1outputstatus {
	XHTTP1_OUTPUT_ERROR = -1,
	XHTTP1_OUTPUT_AGAIN = 0,
	XHTTP1_OUTPUT_DATA = 1,
	XHTTP1_OUTPUT_DONE = 2,
	XHTTP1_OUTPUT_CONTINUE = 3
} xhttp1outputstatus;



/* 入站状态只终结一条响应；升级后的字节不再按 HTTP 解释。 */
typedef enum xhttp1feedstatus {
	XHTTP1_FEED_ERROR = -1,
	XHTTP1_FEED_MORE = 0,
	XHTTP1_FEED_PAUSED = 1,
	XHTTP1_FEED_DONE = 2,
	XHTTP1_FEED_UPGRADED = 3
} xhttp1feedstatus;



/* Exchange 错误码稳定标识请求正文、响应协议和用户回调失败。 */
typedef enum xhttp1exchangeerror {
	XHTTP1_EXCHANGE_ERROR_ARGUMENT = 1,
	XHTTP1_EXCHANGE_ERROR_STATE,
	XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT,
	XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD,
	XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING,
	XHTTP1_EXCHANGE_ERROR_INFORMATIONAL_LIMIT,
	XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK,
	XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK,
	XHTTP1_EXCHANGE_ERROR_REQUEST_BODY,
	XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH,
	XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF
} xhttp1exchangeerror;



/*
	Informational 的响应只在回调期间借用。
	Headers 和 Body 回调借用最终响应；返回 false 会终止 Exchange。
	回调返回 false 时，Exchange 会保留回调设置的当前错误作为原因。
	所有回调都由 Feed 同步调用，不得递归 Feed 或销毁当前 Exchange。
*/
typedef bool (*xhttp1informationalproc)(
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttp1headersproc)(
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttp1bodyproc)(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
);



/* Body 为空时缓冲正文，非空时流式交付且响应不保存正文副本。 */
typedef struct xhttp1exchangeevents {
	xhttp1informationalproc Informational;
	xhttp1headersproc Headers;
	xhttp1bodyproc Body;
	ptr Data;
} xhttp1exchangeevents;



/* 各限额分别约束线路解析和最终拥有型 Header 存储。 */
typedef struct xhttp1exchangeconfig {
	xhttp1limits Head;
	xhttp1bodylimits Body;
	xhttpheadersconfig Headers;
	xhttpheadersconfig Trailers;
	uint32 MaxInformational;
	bool AllowRawTransferCodings;
} xhttp1exchangeconfig;



XRT_EXTERN_C_BEGIN



/* 初始化公网安全限额；输出可位于合法的未对齐存储。 */
XRT_API void xrtHttp1ExchangeConfigInit(
	xhttp1exchangeconfig* pConfig
);



/*
	创建无 I/O 的单次 HTTP/1.1 Exchange。
	成功时接管 Plan；失败时 Plan 所有权仍属于调用方。
	非空配置和事件表会在返回前复制，不要求自然对齐。
*/
XRT_API xhttp1exchange* xrtHttp1ExchangeCreate(
	xhttp1requestplan* pPlan,
	const xhttp1exchangeconfig* pConfig,
	const xhttp1exchangeevents* pEvents
);



/* 销毁 Exchange、未取走的响应、请求计划和正文 Reader。 */
XRT_API void xrtHttp1ExchangeDestroy(
	xhttp1exchange* pExchange
);



/*
	借出下一段线路数据，大小不超过 MaxBytes。
	DATA 必须通过 OutputConsume 推进；AGAIN 表示正文源暂不可读。
*/
XRT_API xhttp1outputstatus xrtHttp1ExchangeOutput(
	xhttp1exchange* pExchange,
	size_t iMaxBytes,
	xbytesview* pData
);



/* 消费最近一次 Output 借出的前缀；允许短写。 */
XRT_API bool xrtHttp1ExchangeOutputConsume(
	xhttp1exchange* pExchange,
	size_t iSize
);



/* 允许 Expect: 100-continue 请求开始发送正文；重复调用是安全的。 */
XRT_API bool xrtHttp1ExchangeContinue(
	xhttp1exchange* pExchange
);



/* 暂停响应输入交付；可在 Headers 或 Body 回调内调用。 */
XRT_API bool xrtHttp1ExchangePause(xhttp1exchange* pExchange);



/* 恢复响应输入交付；重复恢复未暂停 Exchange 是安全的。 */
XRT_API bool xrtHttp1ExchangeResume(xhttp1exchange* pExchange);



/* 判断响应输入是否由应用暂停。 */
XRT_API bool xrtHttp1ExchangePaused(
	const xhttp1exchange* pExchange
);



/*
	同步消费一段响应输入；Accepted 返回已经处理或内部保留的前缀。
	End 表示本段之后可靠传输正常结束。
	若 Feed 提前终止请求输出，先前借出的 DATA 仍有效到 Consume 或 Destroy。
*/
XRT_API xhttp1feedstatus xrtHttp1ExchangeFeed(
	xhttp1exchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
);



/* 返回最终响应的借用指针；最终 Header 到达前返回空指针。 */
XRT_API const xhttpresponse* xrtHttp1ExchangeResponse(
	const xhttp1exchange* pExchange
);



/* 在 DONE 或 UPGRADED 后取走最终响应；其他状态失败。 */
XRT_API xhttpresponse* xrtHttp1ExchangeTakeResponse(
	xhttp1exchange* pExchange
);



/* 返回因跨输入边界而内部保留的升级或多余后缀。 */
XRT_API xbytesview xrtHttp1ExchangeRemainder(
	const xhttp1exchange* pExchange
);



/* 返回终态错误；非失败状态返回空指针。 */
XRT_API const xerror* xrtHttp1ExchangeError(
	const xhttp1exchange* pExchange
);



/* 判断请求 Header 和完整正文是否已经全部被调用方消费。 */
XRT_API bool xrtHttp1ExchangeRequestComplete(
	const xhttp1exchange* pExchange
);



/* 判断最终响应是否已经完整结束。 */
XRT_API bool xrtHttp1ExchangeResponseComplete(
	const xhttp1exchange* pExchange
);



/* 判断响应是否已经切换为升级协议或 CONNECT 隧道。 */
XRT_API bool xrtHttp1ExchangeUpgraded(
	const xhttp1exchange* pExchange
);



/*
	判断连接在协议层是否可复用。
	调用方还必须确认 Feed 返回后没有尚未提交的外部输入。
*/
XRT_API bool xrtHttp1ExchangeReusable(
	const xhttp1exchange* pExchange
);



/* 返回已经消费的信息响应数量。 */
XRT_API uint32 xrtHttp1ExchangeInformationalCount(
	const xhttp1exchange* pExchange
);



/* 返回调用方通过 OutputConsume 确认发送的线路字节数。 */
XRT_API uint64 xrtHttp1ExchangeRequestWireBytes(
	const xhttp1exchange* pExchange
);



XRT_EXTERN_C_END

#endif

#endif

