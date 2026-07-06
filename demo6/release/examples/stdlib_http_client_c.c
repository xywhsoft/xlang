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

int main(void)
{
    xwebconfig cfg;
    xwebserver* server;
    xhttpresponse* resp;
    char base[256];
    char url[512];
    uint16 port;

    xrtInit();
    xrtWebConfigInit(&cfg);
    (void)xrtNetAddrParse(&cfg.tBindAddr, "127.0.0.1", 0);

    server = xrtWebServerCreate(NULL, &cfg);
    require_line("create", server != NULL);
    require_line("route_get", xrtWebServerRouteEx(server, xrtWebMethodMask("GET"), "/hello", route, NULL, NULL));
    require_line("route_post", xrtWebServerRouteEx(server, xrtWebMethodMask("POST"), "/echo", route, NULL, NULL));
    require_line("route_method", xrtWebServerRouteEx(server, xrtWebMethodMask("PUT|PATCH"), "/method", route, NULL, NULL));
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

    xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
    xrtNetSyncShutdownHiddenEngine();
    xrtWebServerDestroy(server);
    xrtUnit();
    return 0;
}
