#ifndef XRT_HTTP_CLIENT_FUTURE_H
#define XRT_HTTP_CLIENT_FUTURE_H




#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE))
	#error "XRT HTTP client Future support requires HTTP client and Future bridge support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/*
	Future 成功值是拥有型 HTTP 结果。
	结果同时覆盖普通响应与 Upgrade，避免异步便利层形成能力死角。
*/
typedef struct xhttpresult xhttpresult;



XRT_EXTERN_C_BEGIN



/*
	等待 Client 进入 CLOSED；Future 取消只放弃当前等待，不改变 Client 生命周期。
	Future 在活动 Call 回调、池传输和池 Timer 全部退出后才成功完成。
*/
XRT_API xfuture* xrtHttpClientWaitAsync(xhttpclient* pClient);



/* 增加有效结果引用并返回原指针；空指针或引用计数耗尽时失败。 */
XRT_API xhttpresult* xrtHttpResultRef(xhttpresult* pResult);



/*
	释放结果引用。
	最后一个引用会销毁尚未取走的响应，并中止、释放尚未取走的 Upgrade 传输。
*/
XRT_API void xrtHttpResultDestroy(xhttpresult* pResult);



/* 返回结果借用的响应；结果保持有效且响应未被取走期间可用。 */
XRT_API const xhttpresponse* xrtHttpResultResponse(
	const xhttpresult* pResult
);



/*
	取走响应所有权并清空结果中的响应。
	同一结果的 Take 与借用访问不可并发。
*/
XRT_API xhttpresponse* xrtHttpResultTakeResponse(
	xhttpresult* pResult
);



/* 返回结果借用的明文 Upgrade Stream，普通响应或 HTTPS Upgrade 返回空。 */
XRT_API xnetstream* xrtHttpResultTcp(
	const xhttpresult* pResult
);



/* 取走明文 Upgrade Stream 所有权；同一传输至多成功取走一次。 */
XRT_API xnetstream* xrtHttpResultTakeTcp(
	xhttpresult* pResult
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 返回结果借用的 TLS Upgrade Stream，普通响应或明文 Upgrade 返回空。 */
XRT_API xtlsstream* xrtHttpResultTls(
	const xhttpresult* pResult
);



/* 取走 TLS Upgrade Stream 所有权；同一传输至多成功取走一次。 */
XRT_API xtlsstream* xrtHttpResultTakeTls(
	xhttpresult* pResult
);

#endif



/* 返回 Upgrade 后已经留在传输接收缓冲中的协议外字节数。 */
XRT_API size_t xrtHttpResultBuffered(
	const xhttpresult* pResult
);



/* 返回本次调用已经跟随的重定向次数。 */
XRT_API size_t xrtHttpResultRedirects(
	const xhttpresult* pResult
);



/* 判断结果是否已经把传输从 HTTP/1 交付给调用方。 */
XRT_API bool xrtHttpResultUpgraded(
	const xhttpresult* pResult
);



/* 复制不可变 Call 快照；输出存储可以未对齐但必须完整。 */
XRT_API bool xrtHttpResultInfo(
	const xhttpresult* pResult,
	xhttpcallinfo* pInfo
);



/*
	以 Future 执行一次高层 HTTP 调用。
	Future 成功值是其拥有的 xhttpresult；失败错误由 Future 持有。
	取消 Future 会协作取消底层 Call，外部 Call 取消也会反映为 Future 取消终态。
*/
XRT_API xfuture* xrtHttpClientDoAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
);



/*
	阻塞执行一次高层 HTTP 调用并返回拥有型结果。
	网络 Worker 不允许调用此函数；协程应使用 DoAsync 与 xrtFutureAwait。
*/
XRT_API xhttpresult* xrtHttpClientDoSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
);



XRT_EXTERN_C_END

#endif

#endif
