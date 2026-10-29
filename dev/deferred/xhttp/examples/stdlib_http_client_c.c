#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct stream_state {
	uint16 Status;
	char Body[128];
	size_t BodySize;
	bool AbortBody;
	bool CancelAccepted;
} stream_state;



/* 输出一条检查结果，失败时立即停止示例。 */
static void require_line(cstr sName, bool bResult)
{
	if ( !bResult ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}
	printf("%s=ok\n", sName);
}



/* 比较借用字符串视图和零结尾文本。 */
static bool view_equals(xstrview Value, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Value.Size == iSize) &&
		(memcmp(Value.Data, sExpected, iSize) == 0);
}



/* 判断响应正文是否等于指定文本。 */
static bool response_body_equals(
	const xhttpresponse* pResponse,
	cstr sExpected
)
{
	xbytesview Body;
	size_t iSize;

	if ( pResponse == NULL ) {
		return false;
	}
	Body = xrtHttpResponseBody(pResponse);
	iSize = strlen(sExpected);
	return (Body.Size == iSize) &&
		(memcmp(Body.Data, sExpected, iSize) == 0);
}



/* 判断响应首个同名字段是否等于指定文本。 */
static bool response_header_equals(
	const xhttpresponse* pResponse,
	cstr sName,
	cstr sExpected
)
{
	const xhttpfield* pField;

	if ( pResponse == NULL ) {
		return false;
	}
	pField = xrtHttpResponseHeader(
		pResponse,
		(xstrview){ sName, strlen(sName) }
	);
	return (pField != NULL) && view_equals(pField->Value, sExpected);
}



/* 构造并提交一条可选携带自定义字段的完整响应。 */
static bool send_reply(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body,
	const xhttpfield* pHeader
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(iStatus);
	bool bReady;
	xnetresult Result;

	if ( pReply == NULL ) {
		return false;
	}
	bReady = xrtHttpReplySetBytes(pReply, Body, ContentType);
	if ( bReady && (pHeader != NULL) ) {
		bReady = xrtHttpReplySetHeader(
			pReply,
			pHeader->Name,
			pHeader->Value
		);
	}
	Result = bReady ?
		xrtHttpConnRespond(pConnection, pReply) :
		XNET_RESULT_ERROR;
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* GET /hello 返回固定正文和响应字段。 */
static void route_hello(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	const xhttpfield Header = {
		XRT_STR_LITERAL("X-Reply"),
		XRT_STR_LITERAL("hello")
	};

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	(void)send_reply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("client hello"),
		&Header
	);
}



/* POST /echo 回显正文，并把请求字段复制到响应。 */
static void route_echo(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	const xhttpfield* pRequestHeader;
	xhttpfield Header;

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	pRequestHeader = xrtHttpServerRequestHeader(
		pRequest,
		XRT_STR_LITERAL("X-Test")
	);
	Header.Name = XRT_STR_LITERAL("X-Echo");
	Header.Value = pRequestHeader != NULL ?
		pRequestHeader->Value : XRT_STR_LITERAL("");
	(void)send_reply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		xrtHttpServerRequestBody(pRequest),
		&Header
	);
}



/* PUT/PATCH /method 直接返回实际方法视图。 */
static void route_method(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xstrview Method = xrtHttpServerRequestMethod(pRequest);

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		(xbytesview){ (cbytes)Method.Data, Method.Size }
	);
}



/* GET /meta 返回重复字段，用于验证完整响应元数据。 */
static void route_meta(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xhttpreply* pReply;
	bool bReady;

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	if ( pReply == NULL ) {
		return;
	}
	bReady = xrtHttpReplySetHeader(
		pReply,
		XRT_STR_LITERAL("X-Meta"),
		XRT_STR_LITERAL("yes")
	) && xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("a=1")
	) && xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL("b=2")
	) && xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL("stream-body"),
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	);
	if ( bReady ) {
		(void)xrtHttpConnRespond(pConnection, pReply);
	}
	xrtHttpReplyDestroy(pReply);
}



/* GET /slow 延迟响应，为协作取消提供稳定窗口。 */
static void route_slow(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	xrtSleep(300);
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("slow-body")
	);
}



/* 流式响应头回调只保存稳定元数据。 */
static bool stream_headers(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	stream_state* pState = (stream_state*)pData;

	(void)pCall;
	if ( (pState == NULL) || (pResponse == NULL) ) {
		return false;
	}
	pState->Status = xrtHttpResponseStatus(pResponse);
	return true;
}



/* 流式正文回调消费字节，并可重入请求取消当前 Call。 */
static bool stream_body(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	stream_state* pState = (stream_state*)pData;
	size_t iAvailable;

	(void)pResponse;
	if ( pState == NULL ) {
		return false;
	}
	iAvailable = sizeof(pState->Body) - pState->BodySize;
	if ( Data.Size > iAvailable ) {
		return false;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pState->Body + pState->BodySize,
			Data.Data,
			Data.Size
		);
		pState->BodySize += Data.Size;
	}
	if ( pState->AbortBody && !pState->CancelAccepted ) {
		pState->CancelAccepted = xrtHttpCallCancel(pCall);
	}
	return true;
}



/* 创建当前 HTTP 契约使用的拥有型请求。 */
static xhttprequest* create_request(
	cstr sMethod,
	cstr sUrl,
	cstr sBody,
	cstr sTestHeader
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ sUrl, strlen(sUrl) }
	);

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( (sTestHeader != NULL) && !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("X-Test"),
		(xstrview){ sTestHeader, strlen(sTestHeader) }
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	if ( (sBody != NULL) && !xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)sBody, strlen(sBody) },
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 同步执行请求，并返回拥有型完整结果。 */
static xhttpresult* request_sync(
	xhttpclient* pClient,
	cstr sMethod,
	cstr sUrl,
	cstr sBody,
	cstr sTestHeader
)
{
	xhttprequest* pRequest = create_request(
		sMethod,
		sUrl,
		sBody,
		sTestHeader
	);
	xhttpresult* pResult;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pResult = xrtHttpClientDoSync(pClient, pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	return pResult;
}



/* 创建本地 Router Server，并发布其动态监听端口。 */
static xhttpserver* start_server(
	xnetengine* pEngine,
	uint16* pPort
)
{
	xhttpserverrouter* pRouter;
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpserver* pServer;
	xnetaddr Address;
	bool bReady;

	pRouter = xrtHttpServerRouterCreate(NULL);
	if ( pRouter == NULL ) {
		return NULL;
	}
	bReady = xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/hello"),
		route_hello,
		NULL
	) && xrtHttpServerPost(
		pRouter,
		XRT_STR_LITERAL("/echo"),
		route_echo,
		NULL
	) && xrtHttpServerRoute(
		pRouter,
		XRT_STR_LITERAL("PUT"),
		XRT_STR_LITERAL("/method"),
		route_method,
		NULL
	) && xrtHttpServerRoute(
		pRouter,
		XRT_STR_LITERAL("PATCH"),
		XRT_STR_LITERAL("/method"),
		route_method,
		NULL
	) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/meta"),
		route_meta,
		NULL
	) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/slow"),
		route_slow,
		NULL
	) && xrtHttpServerRouterFreeze(pRouter);
	if ( !bReady ) {
		xrtHttpServerRouterDestroy(pRouter);
		return NULL;
	}

	xrtHttpServerConfigInit(&Config);
	if ( !xrtNetAddrLoopback(
		&Config.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		xrtHttpServerRouterDestroy(pRouter);
		return NULL;
	}
	Config.Network.Listen.AcceptConcurrency = 2u;
	xrtHttpServerEventsInit(&Events);
	pServer = xrtHttpServerRouterStart(
		pEngine,
		&Config,
		pRouter,
		&Events
	);
	xrtHttpServerRouterDestroy(pRouter);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address) ) {
		xrtHttpServerDestroy(pServer);
		return NULL;
	}
	*pPort = Address.Port;
	return pServer;
}



/* 排空并等待 HTTP Client 完整关闭。 */
static void close_client(xhttpclient* pClient)
{
	xfuture* pClosed = xrtHttpClientWaitAsync(pClient);

	require_line(
		"client_drain",
		(pClosed != NULL) && xrtHttpClientDrain(pClient)
	);
	xrtHttpClientDestroy(pClient);
	require_line(
		"client_close",
		(xrtFutureWaitFor(pClosed, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED)
	);
	xrtFutureDestroy(pClosed);
}



/* 排空并等待 HTTP Server 完整关闭。 */
static void close_server(xhttpserver* pServer)
{
	xfuture* pClosed = xrtHttpServerWaitAsync(pServer);

	require_line(
		"server_drain",
		(pClosed != NULL) && xrtHttpServerDrain(pServer)
	);
	xrtHttpServerDestroy(pServer);
	require_line(
		"server_close",
		(xrtFutureWaitFor(pClosed, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtFutureState(pClosed) == XFUTURE_RESOLVED)
	);
	xrtFutureDestroy(pClosed);
}



/* 覆盖同步、异步、流式、取消和诊断契约的本地 HTTP Client 示例。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xhttpclientconfig ClientConfig;
	xhttpcalloptions Options;
	xnetengine* pEngine;
	xhttpserver* pServer;
	xhttpclient* pClient;
	xhttprequest* pRequest;
	xhttpresult* pResult;
	const xhttpresponse* pResponse;
	xfuture* pFuture;
	xhttpcallinfo Info;
	stream_state Stream;
	char sBase[256];
	char sUrl[512];
	uint16 iPort;
	size_t iCookieCount;
	size_t i;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	require_line(
		"engine_start",
		(pEngine != NULL) && xrtNetEngineStart(pEngine)
	);
	pServer = start_server(pEngine, &iPort);
	require_line("server_start", pServer != NULL);
	require_line(
		"base_url",
		snprintf(
			sBase,
			sizeof(sBase),
			"http://127.0.0.1:%u",
			(unsigned)iPort
		) > 0
	);

	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	require_line("client_create", pClient != NULL);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/hello", sBase);
	pResult = request_sync(pClient, "GET", sUrl, NULL, NULL);
	pResponse = xrtHttpResultResponse(pResult);
	require_line(
		"http_get",
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == XHTTP_STATUS_OK) &&
		response_header_equals(pResponse, "X-Reply", "hello") &&
		response_body_equals(pResponse, "client hello")
	);
	xrtHttpResultDestroy(pResult);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/echo", sBase);
	pResult = request_sync(
		pClient,
		"POST",
		sUrl,
		"payload",
		"client"
	);
	pResponse = xrtHttpResultResponse(pResult);
	require_line(
		"http_request_object",
		(pResponse != NULL) &&
		response_header_equals(pResponse, "X-Echo", "client") &&
		response_body_equals(pResponse, "payload")
	);
	xrtHttpResultDestroy(pResult);

	pResult = request_sync(pClient, "POST", sUrl, "via-post", NULL);
	pResponse = xrtHttpResultResponse(pResult);
	require_line(
		"http_post",
		(pResponse != NULL) && response_body_equals(pResponse, "via-post")
	);
	xrtHttpResultDestroy(pResult);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/method", sBase);
	pResult = request_sync(pClient, "PUT", sUrl, NULL, NULL);
	pResponse = xrtHttpResultResponse(pResult);
	require_line(
		"http_request_method",
		(pResponse != NULL) && response_body_equals(pResponse, "PUT")
	);
	xrtHttpResultDestroy(pResult);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/meta", sBase);
	pResult = request_sync(pClient, "GET", sUrl, NULL, NULL);
	pResponse = xrtHttpResultResponse(pResult);
	iCookieCount = 0;
	for ( i = 0; (pResponse != NULL) &&
		(i < xrtHttpResponseHeaderCount(pResponse)); ++i ) {
		const xhttpfield* pField = xrtHttpResponseHeaderAt(pResponse, i);

		if ( (pField != NULL) &&
			view_equals(pField->Name, "Set-Cookie") ) {
			++iCookieCount;
		}
	}
	require_line(
		"http_response_metadata",
		(pResponse != NULL) &&
		(xrtHttpResponseVersion(pResponse) == XHTTP_VERSION_1_1) &&
		(iCookieCount == 2u) &&
		response_body_equals(pResponse, "stream-body")
	);
	require_line(
		"http_call_info",
		xrtHttpResultInfo(pResult, &Info) &&
		(Info.State == XHTTP_CALL_SUCCEEDED) &&
		(Info.Phase == XHTTP_CALL_PHASE_RESPONSE_BODY) &&
		(Info.Result == XNET_RESULT_OK) &&
		(Info.Error == XHTTP_CLIENT_ERROR_NONE) &&
		(Info.ResponseBodyBytes == 11u) &&
		(Info.Completed >= Info.Submitted)
	);
	xrtHttpResultDestroy(pResult);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/hello", sBase);
	pRequest = create_request("GET", sUrl, NULL, NULL);
	pFuture = pRequest != NULL ?
		xrtHttpClientDoAsync(pClient, pRequest, NULL) : NULL;
	xrtHttpRequestDestroy(pRequest);
	require_line(
		"http_async_wait",
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED)
	);
	pResult = (xhttpresult*)xrtFutureValue(pFuture);
	require_line(
		"http_async",
		response_body_equals(
			xrtHttpResultResponse(pResult),
			"client hello"
		)
	);
	xrtFutureDestroy(pFuture);

	memset(&Stream, 0, sizeof(Stream));
	xrtHttpCallOptionsInit(&Options);
	Options.Events.Headers = stream_headers;
	Options.Events.Body = stream_body;
	Options.Events.Data = &Stream;
	(void)snprintf(sUrl, sizeof(sUrl), "%s/meta", sBase);
	pRequest = create_request("GET", sUrl, NULL, NULL);
	pFuture = pRequest != NULL ?
		xrtHttpClientDoAsync(pClient, pRequest, &Options) : NULL;
	xrtHttpRequestDestroy(pRequest);
	require_line(
		"http_stream_wait",
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED)
	);
	require_line(
		"http_stream",
		(Stream.Status == XHTTP_STATUS_OK) &&
		(Stream.BodySize == 11u) &&
		(memcmp(Stream.Body, "stream-body", 11u) == 0)
	);
	xrtFutureDestroy(pFuture);

	memset(&Stream, 0, sizeof(Stream));
	Stream.AbortBody = true;
	Options.Events.Data = &Stream;
	pRequest = create_request("GET", sUrl, NULL, NULL);
	pFuture = pRequest != NULL ?
		xrtHttpClientDoAsync(pClient, pRequest, &Options) : NULL;
	xrtHttpRequestDestroy(pRequest);
	require_line(
		"http_stream_abort_wait",
		(pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(5000000)) == XWAIT_OK)
	);
	require_line(
		"http_stream_abort",
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED) &&
		Stream.CancelAccepted &&
		(Stream.BodySize != 0)
	);
	xrtFutureDestroy(pFuture);

	(void)snprintf(sUrl, sizeof(sUrl), "%s/slow", sBase);
	pRequest = create_request("GET", sUrl, NULL, NULL);
	pFuture = pRequest != NULL ?
		xrtHttpClientDoAsync(pClient, pRequest, NULL) : NULL;
	xrtHttpRequestDestroy(pRequest);
	require_line(
		"http_async_cancel_request",
		(pFuture != NULL) && xrtFutureCancel(pFuture)
	);
	require_line(
		"http_async_cancel",
		(xrtFutureWaitFor(pFuture, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_CANCELLED)
	);
	xrtFutureDestroy(pFuture);

	close_client(pClient);
	close_server(pServer);
	require_line("engine_close", xrtNetEngineDestroy(pEngine));
	return 0;
}
