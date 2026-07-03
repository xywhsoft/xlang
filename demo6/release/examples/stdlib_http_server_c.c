#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct HttpServerExampleState {
    int getCount;
    int postCount;
    int anyCount;
} HttpServerExampleState;

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static xwebaction handle_user(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    char id[64];
    char name[64];
    int ok;

    if ( state ) {
        state->getCount++;
    }

    ok =
        xrtWebRequestParam(req, "id", id, sizeof(id), NULL) &&
        xrtWebRequestQueryValue(req, "name", name, sizeof(name), NULL) &&
        strcmp(id, "42") == 0 &&
        strcmp(name, "xlang") == 0;

    return xrtWebResponseText(resp, ok ? "route-get-ok" : "route-get-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_echo(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    const char* body;
    size_t bodyLen = 0;

    if ( state ) {
        state->postCount++;
    }

    body = (const char*)xrtWebRequestBody(req, &bodyLen);
    if ( body && bodyLen == 4 && memcmp(body, "ping", 4) == 0 ) {
        return xrtWebResponseText(resp, "xlang-http-ok", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }
    return xrtWebResponseText(resp, "xlang-http-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_any(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    if ( state ) {
        state->anyCount++;
    }
    return xrtWebResponseText(resp, xrtWebRequestMethod(req), "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xhttpresponse* request_text(const char* url, const char* method, const char* body)
{
    xhttprequest req;
    xhttpresponse* resp;
    xnet_result status = XRT_NET_ERROR;

    xrtHttpRequestInit(&req);
    (void)xrtHttpRequestSetURL(&req, url);
    if ( method ) {
        (void)xrtHttpRequestSetMethod(&req, method);
    }
    if ( body ) {
        (void)xrtHttpRequestSetBodyCopy(&req, body, strlen(body), "text/plain");
    }

    resp = xrtHttpExecuteSync(NULL, &req, &status);
    xrtHttpRequestUnit(&req);

    if ( status != XRT_NET_OK ) {
        if ( resp ) {
            xrtHttpResponseDestroy(resp);
        }
        return NULL;
    }
    return resp;
}

static void require_response(const char* name, xhttpresponse* resp, unsigned code, const char* body)
{
    require_line(name,
        resp != NULL &&
        resp->iStatusCode == code &&
        resp->pBody != NULL &&
        strstr(resp->pBody, body) != NULL);
    if ( resp ) {
        xrtHttpResponseDestroy(resp);
    }
}

int main(void)
{
    HttpServerExampleState state;
    xwebconfig cfg;
    xwebstaticconfig staticCfg;
    xwebserver* server = NULL;
    xhttpresponse* resp = NULL;
    char url[512];
    uint16 port;

    memset(&state, 0, sizeof(state));
    xrtInit();

    xrtDirCreateAll("release/tmp/example_http_server_c_static");
    xrtFileWriteAll("release/tmp/example_http_server_c_static/index.txt", "static-ok", 9, XRT_CP_UTF8);

    xrtWebConfigInit(&cfg);
    (void)xrtNetAddrParse(&cfg.tBindAddr, "127.0.0.1", 0);
    server = xrtWebServerCreate(NULL, &cfg);
    require_line("create", server != NULL);

    require_line("route_get", xrtWebServerGet(server, "/users/{id}", handle_user, &state));
    require_line("route_post", xrtWebServerPost(server, "/demo", handle_echo, &state));
    require_line("route_any", xrtWebServerAny(server, "/any", handle_any, &state));

    xrtWebStaticConfigInit(&staticCfg);
    require_line("static_mount", xrtWebServerStatic(server, "/public", "release/tmp/example_http_server_c_static", &staticCfg));
    require_line("start", xrtWebServerStart(server) == XRT_NET_OK);

    port = xrtWebServerPort(server);
    require_line("port", port > 0);

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/users/42?name=xlang", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("get_response", resp, 200, "route-get-ok");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/demo?x=1", (unsigned)port);
    resp = request_text(url, "POST", "ping");
    require_response("post_response", resp, 200, "xlang-http-ok");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/any", (unsigned)port);
    resp = request_text(url, "PUT", NULL);
    require_response("any_response", resp, 200, "PUT");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/public/index.txt", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("static_response", resp, 200, "static-ok");

    require_line("callback_count", state.getCount == 1 && state.postCount == 1 && state.anyCount == 1);

    xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
    xrtNetSyncShutdownHiddenEngine();
    xrtWebServerDestroy(server);
    xrtFileDelete("release/tmp/example_http_server_c_static/index.txt");
    xrtDirDelete("release/tmp/example_http_server_c_static");
    xrtUnit();
    return 0;
}
