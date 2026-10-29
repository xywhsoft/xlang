#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct http_server_example {
	xatomic32 GetCount;
	xatomic32 PostCount;
	xatomic32 AnyCount;
	xatomic32 StreamCount;
	char StreamBody[256];
	size_t StreamBodySize;
} http_server_example;



/* 输出一条检查结果，失败时立即停止示例。 */
static void require_line(cstr sName, bool bResult)
{
	if ( !bResult ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}
	printf("%s=ok\n", sName);
}



/* 比较字符串视图和零结尾文本。 */
static bool view_equals(xstrview Value, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	return (Value.Size == iSize) &&
		(memcmp(Value.Data, sExpected, iSize) == 0);
}



/* 判断字节视图是否包含指定文本。 */
static bool bytes_contains(xbytesview Data, cstr sExpected)
{
	size_t iSize = strlen(sExpected);
	size_t i;

	if ( iSize == 0 ) {
		return true;
	}
	if ( (Data.Data == NULL) || (Data.Size < iSize) ) {
		return false;
	}
	for ( i = 0; i <= (Data.Size - iSize); ++i ) {
		if ( memcmp(Data.Data + i, sExpected, iSize) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 一次调用提交常用文本响应。 */
static bool send_text(
	xhttpconn* pConnection,
	uint16 iStatus,
	cstr sText
)
{
	return xrtHttpConnReply(
		pConnection,
		iStatus,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		(xbytesview){ (cbytes)sText, strlen(sText) }
	) == XNET_RESULT_OK;
}



/* GET 路由组合校验路径参数、查询参数和 Cookie。 */
static void handle_user(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;
	xqueryparams* pQuery;
	xquerypair Name;
	xcookiepair Cookie;
	size_t iErrorOffset = 0;
	bool bValid;

	(void)pServer;
	pQuery = xrtHttpServerRequestQueryParams(
		pRequest,
		NULL,
		&iErrorOffset
	);
	bValid = (pQuery != NULL) &&
		(iParamCount == 1u) &&
		view_equals(pParams[0].Name, "id") &&
		view_equals(pParams[0].Value, "42") &&
		xrtQueryParamsGet(
			pQuery,
			XRT_STR_LITERAL("name"),
			&Name
		) &&
		((Name.Flags & XQUERY_HAS_VALUE) != 0) &&
		view_equals(Name.Value, "xlang") &&
		(xrtHttpServerRequestCookie(
			pRequest,
			XRT_STR_LITERAL("sid"),
			&Cookie
		 ) == XCOOKIE_NEXT_ITEM) &&
		view_equals(Cookie.Value, "abc") &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) != 0);
	xrtQueryParamsDestroy(pQuery);
	(void)xrtAtomic32FetchAdd(
		&pState->GetCount,
		1,
		XMEMORY_RELEASE
	);
	(void)send_text(
		pConnection,
		XHTTP_STATUS_OK,
		bValid ? "route-get-ok" : "route-get-bad"
	);
}



/* POST 路由读取已经缓冲的连续正文。 */
static void handle_echo(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;
	xbytesview Body = xrtHttpServerRequestBody(pRequest);
	bool bValid = (Body.Size == 4u) &&
		(memcmp(Body.Data, "ping", 4u) == 0);

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)xrtAtomic32FetchAdd(
		&pState->PostCount,
		1,
		XMEMORY_RELEASE
	);
	(void)send_text(
		pConnection,
		XHTTP_STATUS_OK,
		bValid ? "xlang-http-ok" : "xlang-http-bad"
	);
}



/* URL encoded 表单复用公开 QueryParams 容器。 */
static void handle_form(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xqueryparams* pForm;
	xquerypair Name;
	xquerypair City;
	size_t iErrorOffset = 0;
	bool bValid;

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	pForm = xrtHttpServerRequestForm(
		pRequest,
		NULL,
		&iErrorOffset
	);
	bValid = (pForm != NULL) &&
		(xrtQueryParamsCount(pForm) == 2u) &&
		xrtQueryParamsGet(
			pForm,
			XRT_STR_LITERAL("name"),
			&Name
		) &&
		xrtQueryParamsGet(
			pForm,
			XRT_STR_LITERAL("city"),
			&City
		) &&
		view_equals(Name.Value, "alice bob") &&
		view_equals(City.Value, "shanghai");
	xrtQueryParamsDestroy(pForm);
	(void)send_text(
		pConnection,
		XHTTP_STATUS_OK,
		bValid ? "form-ok" : "form-bad"
	);
}



/* multipart/form-data 使用公开 FormData 和正文视图检查字段。 */
static void handle_multipart(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xmultiparterrorinfo Error;
	xformdata* pForm;
	xformdatapart Title;
	xformdatapart Upload;
	xbytesview TitleBody;
	xbytesview UploadBody;
	bool bValid;

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	memset(&Error, 0, sizeof(Error));
	pForm = xrtHttpServerRequestFormData(
		pRequest,
		NULL,
		NULL,
		&Error
	);
	memset(&Title, 0, sizeof(Title));
	memset(&Upload, 0, sizeof(Upload));
	memset(&TitleBody, 0, sizeof(TitleBody));
	memset(&UploadBody, 0, sizeof(UploadBody));
	bValid = (pForm != NULL) &&
		(xrtFormDataCount(pForm) == 2u) &&
		xrtFormDataGet(
			pForm,
			XRT_STR_LITERAL("title"),
			&Title
		) &&
		xrtFormDataGet(
			pForm,
			XRT_STR_LITERAL("upload"),
			&Upload
		) &&
		xrtHttpBodyView(Title.Body, &TitleBody) &&
		xrtHttpBodyView(Upload.Body, &UploadBody) &&
		((Title.Flags & XFORM_DATA_PART_FILENAME) == 0) &&
		((Upload.Flags & XFORM_DATA_PART_FILENAME) != 0) &&
		view_equals(Upload.Filename, "note.txt") &&
		view_equals(Upload.ContentType, "text/plain") &&
		(TitleBody.Size == 15u) &&
		(memcmp(TitleBody.Data, "hello multipart", 15u) == 0) &&
		(UploadBody.Size == 9u) &&
		(memcmp(UploadBody.Data, "file-data", 9u) == 0);
	xrtFormDataDestroy(pForm);
	(void)send_text(
		pConnection,
		XHTTP_STATUS_OK,
		bValid ? "multipart-ok" : "multipart-bad"
	);
}



/* 流式路由在 Header 阶段选择不缓冲正文。 */
static xhttpserverbodypolicy stream_headers(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	if ( (pState == NULL) ||
		(iParamCount != 1u) ||
		!view_equals(pParams[0].Value, "upload") ) {
		return XHTTP_SERVER_BODY_REJECT;
	}
	pState->StreamBodySize = 0;
	return XHTTP_SERVER_BODY_STREAM;
}



/* 流式路由逐块消费正文，不创建第二份请求缓冲。 */
static bool stream_body(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xbytesview Data,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;

	(void)pServer;
	(void)pConnection;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	if ( (pState == NULL) ||
		(Data.Size > (sizeof(pState->StreamBody) -
		 pState->StreamBodySize)) ) {
		return false;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pState->StreamBody + pState->StreamBodySize,
			Data.Data,
			Data.Size
		);
		pState->StreamBodySize += Data.Size;
	}
	return true;
}



/* 流式请求完成后组合路径参数和已消费正文。 */
static void stream_request(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;
	char sText[384];
	int iWritten;

	(void)pServer;
	(void)pRequest;
	iWritten = snprintf(
		sText,
		sizeof(sText),
		"stream:%.*s:%.*s",
		(int)(iParamCount != 0 ? pParams[0].Value.Size : 0),
		iParamCount != 0 ? pParams[0].Value.Data : "",
		(int)pState->StreamBodySize,
		pState->StreamBody
	);
	if ( (iWritten > 0) && ((size_t)iWritten < sizeof(sText)) ) {
		(void)send_text(pConnection, XHTTP_STATUS_OK, sText);
	}
	(void)xrtAtomic32FetchAdd(
		&pState->StreamCount,
		1,
		XMEMORY_RELEASE
	);
}



/* 任意方法路由直接返回实际方法。 */
static void handle_any(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	http_server_example* pState = (http_server_example*)pData;
	xstrview Method = xrtHttpServerRequestMethod(pRequest);

	(void)pServer;
	(void)pParams;
	(void)iParamCount;
	(void)xrtAtomic32FetchAdd(
		&pState->AnyCount,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		(xbytesview){ (cbytes)Method.Data, Method.Size }
	);
}



/* Cookie 路由演示直接使用已经校验的 Set-Cookie 字段。 */
static void handle_cookie(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	bool bReady;

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	if ( pReply == NULL ) {
		return;
	}
	bReady = xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Set-Cookie"),
		XRT_STR_LITERAL(
			"sid=example; Path=/; Max-Age=3600; "
			"SameSite=Lax; HttpOnly"
		)
	) && xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL("cookie set"),
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	);
	if ( bReady ) {
		(void)xrtHttpConnRespond(pConnection, pReply);
	}
	xrtHttpReplyDestroy(pReply);
}



/* Reply 路由在提交前检查可组合的状态、字段和正文。 */
static void handle_response_meta(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	xhttpreply* pReply = xrtHttpReplyCreate(XHTTP_STATUS_OK);
	const xhttpfield* pFirst;
	xbytesview Body;
	bool bReady;

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	if ( pReply == NULL ) {
		return;
	}
	bReady = xrtHttpReplySetReason(
		pReply,
		XRT_STR_LITERAL("Metadata")
	) && xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("X-Duplicate"),
		XRT_STR_LITERAL("first")
	) && xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("X-Duplicate"),
		XRT_STR_LITERAL("second")
	) && xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL("meta-body"),
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	);
	pFirst = bReady ? xrtHttpReplyHeader(
		pReply,
		XRT_STR_LITERAL("X-Duplicate")
	) : NULL;
	memset(&Body, 0, sizeof(Body));
	bReady = bReady &&
		(xrtHttpReplyStatus(pReply) == XHTTP_STATUS_OK) &&
		view_equals(xrtHttpReplyReason(pReply), "Metadata") &&
		(pFirst != NULL) &&
		view_equals(pFirst->Value, "first") &&
		(xrtHttpReplyHeaderCount(pReply) == 3u) &&
		xrtHttpBodyView(xrtHttpReplyBody(pReply), &Body) &&
		(Body.Size == 9u) &&
		(memcmp(Body.Data, "meta-body", 9u) == 0);
	if ( bReady ) {
		(void)xrtHttpConnRespond(pConnection, pReply);
	}
	xrtHttpReplyDestroy(pReply);
}



/* 原始响应路径允许应用直接提交完整且已验证的 wire bytes。 */
static void handle_raw(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: application/json; charset=utf-8\r\n"
		"Content-Length: 11\r\n"
		"\r\n"
		"{\"ok\":true}";

	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	(void)xrtHttpConnRespondRaw(
		pConnection,
		(xbytesview){
			(cbytes)Response,
			sizeof(Response) - 1u
		},
		XHTTP_SERVER_RAW_KEEP_ALIVE
	);
}



/* 创建拥有型请求，可选复制正文和 Content-Type。 */
static xhttprequest* create_request(
	cstr sUrl,
	cstr sMethod,
	cstr sBody,
	cstr sContentType
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		(xstrview){ sMethod, strlen(sMethod) },
		(xstrview){ sUrl, strlen(sUrl) }
	);

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( !xrtHttpRequestSetHeader(
		pRequest,
		XRT_STR_LITERAL("Cookie"),
		XRT_STR_LITERAL("sid=abc")
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	if ( (sBody != NULL) && !xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)sBody, strlen(sBody) },
		(xstrview){ sContentType, strlen(sContentType) }
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 同步执行一次本地请求并返回拥有型结果。 */
static xhttpresult* request_sync(
	xhttpclient* pClient,
	cstr sUrl,
	cstr sMethod,
	cstr sBody,
	cstr sContentType
)
{
	xhttprequest* pRequest = create_request(
		sUrl,
		sMethod,
		sBody,
		sContentType
	);
	xhttpresult* pResult;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pResult = xrtHttpClientDoSync(pClient, pRequest, NULL);
	xrtHttpRequestDestroy(pRequest);
	return pResult;
}



/* 校验响应状态和正文后释放结果。 */
static void require_response(
	cstr sName,
	xhttpresult* pResult,
	uint16 iStatus,
	cstr sBody
)
{
	const xhttpresponse* pResponse = xrtHttpResultResponse(pResult);

	require_line(
		sName,
		(pResponse != NULL) &&
		(xrtHttpResponseStatus(pResponse) == iStatus) &&
		bytes_contains(xrtHttpResponseBody(pResponse), sBody)
	);
	xrtHttpResultDestroy(pResult);
}



/* 校验响应字段后释放结果。 */
static void require_header_response(
	cstr sName,
	xhttpresult* pResult,
	cstr sHeader,
	cstr sValue
)
{
	const xhttpresponse* pResponse = xrtHttpResultResponse(pResult);
	const xhttpfield* pField = pResponse != NULL ?
		xrtHttpResponseHeader(
			pResponse,
			(xstrview){ sHeader, strlen(sHeader) }
		) : NULL;

	require_line(
		sName,
		(pResponse != NULL) &&
		(pField != NULL) &&
		bytes_contains(
			(xbytesview){
				(cbytes)pField->Value.Data,
				pField->Value.Size
			},
			sValue
		)
	);
	xrtHttpResultDestroy(pResult);
}



/* 注册并冻结完整 Router。 */
static xhttpserverrouter* create_router(http_server_example* pState)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);
	xhttpserverrouteevents Stream;
	bool bReady;

	if ( pRouter == NULL ) {
		return NULL;
	}
	xrtHttpServerRouteEventsInit(&Stream);
	Stream.Headers = stream_headers;
	Stream.Body = stream_body;
	Stream.Request = stream_request;
	Stream.Data = pState;
	bReady = xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/users/{id}"),
		handle_user,
		pState
	) && xrtHttpServerPost(
		pRouter,
		XRT_STR_LITERAL("/demo"),
		handle_echo,
		pState
	) && xrtHttpServerPost(
		pRouter,
		XRT_STR_LITERAL("/form"),
		handle_form,
		pState
	) && xrtHttpServerPost(
		pRouter,
		XRT_STR_LITERAL("/multipart"),
		handle_multipart,
		pState
	) && xrtHttpServerRouteEvents(
		pRouter,
		XRT_STR_LITERAL("POST"),
		XRT_STR_LITERAL("/stream/{name}"),
		&Stream
	) && xrtHttpServerAny(
		pRouter,
		XRT_STR_LITERAL("/any"),
		handle_any,
		pState
	) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/cookie"),
		handle_cookie,
		NULL
	) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/response-meta"),
		handle_response_meta,
		NULL
	) && xrtHttpServerGet(
		pRouter,
		XRT_STR_LITERAL("/raw"),
		handle_raw,
		NULL
	) && xrtHttpServerRouterFreeze(pRouter);
	if ( !bReady ) {
		xrtHttpServerRouterDestroy(pRouter);
		return NULL;
	}
	return pRouter;
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



/* 运行本地 HTTP Server，并从真实 Client 验证公开服务端契约。 */
int main(void)
{
	static const char MultipartBody[] =
		"--xlang\r\n"
		"Content-Disposition: form-data; name=\"title\"\r\n"
		"\r\n"
		"hello multipart\r\n"
		"--xlang\r\n"
		"Content-Disposition: form-data; name=\"upload\"; "
		"filename=\"note.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"file-data\r\n"
		"--xlang--\r\n";
	http_server_example State;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpserverevents ServerEvents;
	xhttpclientconfig ClientConfig;
	xnetengine* pEngine;
	xhttpserverrouter* pRouter;
	xhttpserver* pServer;
	xhttpclient* pClient;
	xhttpresult* pResult;
	xnetaddr Address;
	char sUrl[512];

	memset(&State, 0, sizeof(State));
	xrtAtomic32Init(&State.GetCount, 0);
	xrtAtomic32Init(&State.PostCount, 0);
	xrtAtomic32Init(&State.AnyCount, 0);
	xrtAtomic32Init(&State.StreamCount, 0);

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	require_line(
		"engine_start",
		(pEngine != NULL) && xrtNetEngineStart(pEngine)
	);

	pRouter = create_router(&State);
	require_line("router_create", pRouter != NULL);
	xrtHttpServerConfigInit(&ServerConfig);
	require_line(
		"listen_address",
		xrtNetAddrLoopback(
			&ServerConfig.Network.Listen.Address,
			XNET_FAMILY_IPV4,
			0
		)
	);
	ServerConfig.Network.Listen.AcceptConcurrency = 2u;
	xrtHttpServerEventsInit(&ServerEvents);
	pServer = xrtHttpServerRouterStart(
		pEngine,
		&ServerConfig,
		pRouter,
		&ServerEvents
	);
	xrtHttpServerRouterDestroy(pRouter);
	require_line(
		"server_start",
		(pServer != NULL) &&
		xrtHttpServerLocal(pServer, 0, &Address) &&
		(Address.Port != 0)
	);

	xrtHttpClientConfigInit(&ClientConfig);
	pClient = xrtHttpClientCreate(pEngine, &ClientConfig);
	require_line("client_create", pClient != NULL);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/users/42?name=xlang",
		(unsigned)Address.Port
	);
	require_response(
		"get_response",
		request_sync(pClient, sUrl, "GET", NULL, NULL),
		XHTTP_STATUS_OK,
		"route-get-ok"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/demo",
		(unsigned)Address.Port
	);
	require_response(
		"post_response",
		request_sync(pClient, sUrl, "POST", "ping", "text/plain"),
		XHTTP_STATUS_OK,
		"xlang-http-ok"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/form",
		(unsigned)Address.Port
	);
	require_response(
		"form_response",
		request_sync(
			pClient,
			sUrl,
			"POST",
			"name=alice+bob&city=shanghai",
			"application/x-www-form-urlencoded; charset=UTF-8"
		),
		XHTTP_STATUS_OK,
		"form-ok"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/multipart",
		(unsigned)Address.Port
	);
	require_response(
		"multipart_response",
		request_sync(
			pClient,
			sUrl,
			"POST",
			MultipartBody,
			"multipart/form-data; boundary=xlang"
		),
		XHTTP_STATUS_OK,
		"multipart-ok"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/stream/upload",
		(unsigned)Address.Port
	);
	require_response(
		"stream_response",
		request_sync(
			pClient,
			sUrl,
			"POST",
			"hello-world",
			"text/plain"
		),
		XHTTP_STATUS_OK,
		"stream:upload:hello-world"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/any",
		(unsigned)Address.Port
	);
	require_response(
		"any_response",
		request_sync(pClient, sUrl, "PUT", NULL, NULL),
		XHTTP_STATUS_OK,
		"PUT"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/cookie",
		(unsigned)Address.Port
	);
	pResult = request_sync(pClient, sUrl, "GET", NULL, NULL);
	require_header_response(
		"cookie_response",
		pResult,
		"Set-Cookie",
		"sid=example; Path=/; Max-Age=3600"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/response-meta",
		(unsigned)Address.Port
	);
	require_response(
		"response_meta",
		request_sync(pClient, sUrl, "GET", NULL, NULL),
		XHTTP_STATUS_OK,
		"meta-body"
	);

	(void)snprintf(
		sUrl,
		sizeof(sUrl),
		"http://127.0.0.1:%u/raw",
		(unsigned)Address.Port
	);
	require_response(
		"raw_response",
		request_sync(pClient, sUrl, "GET", NULL, NULL),
		XHTTP_STATUS_OK,
		"{\"ok\":true}"
	);

	require_line(
		"callback_count",
		(xrtAtomic32Load(
			&State.GetCount,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.PostCount,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.AnyCount,
			XMEMORY_ACQUIRE
		 ) == 1) &&
		(xrtAtomic32Load(
			&State.StreamCount,
			XMEMORY_ACQUIRE
		 ) == 1)
	);

	close_client(pClient);
	close_server(pServer);
	require_line("engine_close", xrtNetEngineDestroy(pEngine));
	return 0;
}
