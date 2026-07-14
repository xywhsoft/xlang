#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

typedef struct stream_state {
    uint32 status;
    char body[128];
    size_t body_size;
    int abort_body;
} stream_state;

static bool stream_headers(ptr user_data, const xhttpresponse* response)
{
    stream_state* state = (stream_state*)user_data;
    if ( state == NULL || response == NULL ) {
        return false;
    }
    state->status = xrtHttpResponseStatusCode(response);
    return true;
}

static bool stream_body(ptr user_data, const void* data, size_t size)
{
    stream_state* state = (stream_state*)user_data;
    size_t available;
    if ( state == NULL || (data == NULL && size != 0u) ) {
        return false;
    }
    available = sizeof(state->body) - state->body_size;
    if ( size > available ) {
        size = available;
    }
    if ( size != 0u ) {
        memcpy(state->body + state->body_size, data, size);
        state->body_size += size;
    }
    return state->abort_body == 0;
}

static xwebaction route(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    const char* method = xrtWebRequestMethod(req);
    const char* path = xrtWebRequestPath(req);
    const char* body;
    const char* testHeader;
    size_t bodyLen = 0u;

    (void)userData;

    if ( strcmp(method, "GET") == 0 && strcmp(path, "/hello") == 0 ) {
        (void)xrtWebResponseSetHeader(resp, "X-Reply", "hello");
        return xrtWebResponseText(resp, "client hello", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }

    if ( strcmp(method, "POST") == 0 && strcmp(path, "/echo") == 0 ) {
        body = (const char*)xrtWebRequestBody(req, &bodyLen);
        testHeader = xrtWebRequestHeader(req, "X-Test");
        (void)xrtWebResponseSetHeader(resp, "X-Echo", testHeader != NULL ? testHeader : "");
        return xrtWebResponseBody(resp, body != NULL ? body : "", bodyLen, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }

    if ( strcmp(path, "/method") == 0 ) {
        return xrtWebResponseText(resp, method, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }

    if ( strcmp(path, "/meta") == 0 ) {
        (void)xrtWebResponseSetHeader(resp, "X-Meta", "yes");
        (void)xrtWebResponseAddHeader(resp, "Set-Cookie", "a=1");
        (void)xrtWebResponseAddHeader(resp, "Set-Cookie", "b=2");
        return xrtWebResponseText(resp, "stream-body", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }

    if ( strcmp(path, "/slow") == 0 ) {
        xrtSleep(300);
        return xrtWebResponseText(resp, "slow-body", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }

    xrtWebResponseStatus(resp, 404, NULL);
    return xrtWebResponseText(resp, "not found", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xhttpresponse* request_text(const char* method, const char* url, const char* body, const char* testHeader)
{
    xhttprequest* req;
    xhttpresponse* resp;
    xnet_result status = XRT_NET_ERROR;

    req = xrtHttpRequestCreate();
    if ( req == NULL ) {
        return NULL;
    }

    (void)xrtHttpRequestSetURL(req, url);
    (void)xrtHttpRequestSetMethod(req, method);
    if ( testHeader != NULL ) {
        (void)xrtHttpRequestSetHeader(req, "X-Test", testHeader);
    }
    if ( body != NULL ) {
        (void)xrtHttpRequestSetBodyCopy(req, body, strlen(body), "text/plain; charset=utf-8");
    }

    resp = xrtHttpExecuteSync(NULL, req, &status);
    xrtHttpRequestDestroy(req);
    if ( status != XRT_NET_OK ) {
        if ( resp != NULL ) {
            xrtHttpResponseDestroy(resp);
        }
        return NULL;
    }
    return resp;
}

static int response_body_equals(xhttpresponse* resp, const char* expected)
{
    char* body;
    int ok;

    body = xrtHttpResponseBodyTextCopy(resp);
    ok = body != NULL && strcmp(body, expected) == 0;
    xrtFree(body);
    return ok;
}

static xhttprequest* create_request(const char* method, const char* url)
{
    xhttprequest* request = xrtHttpRequestCreate();
    if ( request == NULL ||
         !xrtHttpRequestSetMethod(request, method) ||
         !xrtHttpRequestSetURL(request, url) ) {
        xrtHttpRequestDestroy(request);
        return NULL;
    }
    return request;
}

int main(void)
{
    xwebconfig cfg;
    xwebserver* server;
    xhttpresponse* resp;
    xhttpclient* client;
    xhttprequest* req;
    xnetfuture* future;
    xnet_result status;
    xhttpstreamcallbacks callbacks;
    stream_state stream;
    char base[256];
    char url[512];
    uint16 port;
    uint32 i;
    uint32 cookie_count;

    xrtInit();
    xrtWebConfigInit(&cfg);
    (void)xrtNetAddrParse(&cfg.tBindAddr, "127.0.0.1", 0);

    server = xrtWebServerCreate(NULL, &cfg);
    require_line("create", server != NULL);
    require_line("route_get", xrtWebServerRouteEx(server, xrtWebMethodMask("GET"), "/hello", route, NULL, NULL));
    require_line("route_post", xrtWebServerRouteEx(server, xrtWebMethodMask("POST"), "/echo", route, NULL, NULL));
    require_line("route_method", xrtWebServerRouteEx(server, xrtWebMethodMask("PUT|PATCH"), "/method", route, NULL, NULL));
    require_line("route_meta", xrtWebServerRouteEx(server, xrtWebMethodMask("GET"), "/meta", route, NULL, NULL));
    require_line("route_slow", xrtWebServerRouteEx(server, xrtWebMethodMask("GET"), "/slow", route, NULL, NULL));
    require_line("start", xrtWebServerStart(server) == XRT_NET_OK);

    port = xrtWebServerPort(server);
    snprintf(base, sizeof(base), "http://127.0.0.1:%u", (unsigned)port);
    xrtSleep(20);

    snprintf(url, sizeof(url), "%s/hello", base);
    resp = request_text("GET", url, NULL, NULL);
    require_line(
        "http_get",
        resp != NULL &&
            xrtHttpResponseStatusCode(resp) == 200u &&
            strcmp(xrtHttpResponseHeader(resp, "X-Reply"), "hello") == 0 &&
            response_body_equals(resp, "client hello") &&
            xrtHttpResponseContentLength(resp) == 12 &&
            xrtHttpResponseBody(resp, NULL) != NULL);
    xrtHttpResponseDestroy(resp);

    snprintf(url, sizeof(url), "%s/echo", base);
    resp = request_text("POST", url, "payload", "client");
    require_line(
        "http_request_object",
        resp != NULL &&
            xrtHttpResponseStatusCode(resp) == 200u &&
            strcmp(xrtHttpResponseHeader(resp, "X-Echo"), "client") == 0 &&
            response_body_equals(resp, "payload"));
    xrtHttpResponseDestroy(resp);

    snprintf(url, sizeof(url), "%s/echo", base);
    resp = request_text("POST", url, "via-post", NULL);
    require_line(
        "http_post",
        resp != NULL &&
            xrtHttpResponseStatusCode(resp) == 200u &&
            response_body_equals(resp, "via-post") &&
            xrtHttpResponseContentLength(resp) == 8);
    xrtHttpResponseDestroy(resp);

    snprintf(url, sizeof(url), "%s/method", base);
    resp = request_text("PUT", url, NULL, NULL);
    require_line(
        "http_request_method",
        resp != NULL &&
            xrtHttpResponseStatusCode(resp) == 200u &&
            response_body_equals(resp, "PUT"));
    xrtHttpResponseDestroy(resp);

    /* 可复用客户端公开协议元数据、重复响应头和完整诊断。 */
    client = xrtHttpClientCreate(NULL);
    require_line("http_client_create", client != NULL);
    snprintf(url, sizeof(url), "%s/meta", base);
    req = create_request("GET", url);
    resp = xrtHttpClientExecuteSync(client, req, &status);
    require_line("http_client_first", resp != NULL && status == XRT_NET_OK);
    xrtHttpResponseDestroy(resp);
    xrtHttpRequestDestroy(req);

    req = create_request("GET", url);
    resp = xrtHttpClientExecuteSync(client, req, &status);
    cookie_count = 0u;
    for ( i = 0u; resp != NULL && i < xrtHttpResponseHeaderCount(resp); ++i ) {
        const char* name = xrtHttpResponseHeaderNameAt(resp, i);
        if ( name != NULL && strcmp(name, "Set-Cookie") == 0 ) {
            ++cookie_count;
        }
    }
    require_line(
        "http_response_metadata",
        resp != NULL && status == XRT_NET_OK &&
            strcmp(xrtHttpResponseVersion(resp), "HTTP/1.1") == 0 &&
            (xrtHttpResponseFlags(resp) & XHTTP_RESP_F_KEEPALIVE) != 0u &&
            cookie_count == 2u && response_body_equals(resp, "stream-body"));
    require_line(
        "http_diagnostics",
        resp != NULL &&
            xrtHttpResponseDiagnostics(resp)->eError == XHTTP_ERROR_NONE &&
            xrtHttpResponseDiagnostics(resp)->ePhase == XHTTP_PHASE_COMPLETE &&
            xrtHttpResponseDiagnostics(resp)->bReusedConnection &&
            xrtHttpResponseDiagnostics(resp)->iResponseBodyBytes == 11u);
    xrtHttpResponseDestroy(resp);
    xrtHttpRequestDestroy(req);

    /* 异步请求与流式请求共用 XRT future，响应所有权交给调用者。 */
    snprintf(url, sizeof(url), "%s/hello", base);
    req = create_request("GET", url);
    future = xrtHttpClientExecuteAsync(client, req);
    status = future != NULL ? xrtNetFutureWait(future, 5000u) : XRT_NET_ERROR;
    resp = status == XRT_NET_OK ? (xhttpresponse*)xrtNetFutureValue(future) : NULL;
    require_line("http_async", resp != NULL && response_body_equals(resp, "client hello"));
    xrtHttpResponseDestroy(resp);
    xrtNetFutureDestroy(future);
    xrtHttpRequestDestroy(req);

    memset(&stream, 0, sizeof(stream));
    xrtHttpStreamCallbacksInit(&callbacks);
    callbacks.pUserData = &stream;
    callbacks.OnHeaders = stream_headers;
    callbacks.OnBody = stream_body;
    snprintf(url, sizeof(url), "%s/meta", base);
    req = create_request("GET", url);
    future = xrtHttpClientExecuteStreamAsync(client, req, &callbacks);
    status = future != NULL ? xrtNetFutureWait(future, 5000u) : XRT_NET_ERROR;
    resp = status == XRT_NET_OK ? (xhttpresponse*)xrtNetFutureValue(future) : NULL;
    require_line(
        "http_stream",
        resp != NULL && stream.status == 200u && stream.body_size == 11u &&
            memcmp(stream.body, "stream-body", 11u) == 0);
    xrtHttpResponseDestroy(resp);
    xrtNetFutureDestroy(future);
    xrtHttpRequestDestroy(req);

    memset(&stream, 0, sizeof(stream));
    stream.abort_body = 1;
    callbacks.pUserData = &stream;
    req = create_request("GET", url);
    future = xrtHttpClientExecuteStreamAsync(client, req, &callbacks);
    status = future != NULL ? xrtNetFutureWait(future, 5000u) : XRT_NET_ERROR;
    require_line("http_stream_abort", status == XRT_NET_CANCELLED && stream.body_size != 0u);
    xrtNetFutureDestroy(future);
    xrtHttpRequestDestroy(req);

    snprintf(url, sizeof(url), "%s/slow", base);
    req = create_request("GET", url);
    future = xrtHttpClientExecuteAsync(client, req);
    require_line("http_async_cancel_request", future != NULL && xrtHttpCancel(future));
    status = future != NULL ? xrtNetFutureWait(future, 5000u) : XRT_NET_ERROR;
    require_line("http_async_cancel", status == XRT_NET_CANCELLED);
    xrtNetFutureDestroy(future);
    xrtHttpRequestDestroy(req);
    xrtHttpClientCloseIdle(client);
    xrtHttpClientDestroy(client);

    xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
    xrtNetSyncShutdownHiddenEngine();
    xrtWebServerDestroy(server);
    xrtUnit();
    return 0;
}
