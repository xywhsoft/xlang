#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct HttpServerExampleState {
    int getCount;
    int postCount;
    int anyCount;
    int streamCount;
    char streamBody[256];
    size_t streamBodyLen;
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
    char* idCopy;
    char* nameCopy;
    char* cookieCopy;
    char* firstHeaderName;
    char* firstHeaderValue;
    char* firstParamName;
    char* firstParamValue;
    char* firstQueryName;
    char* firstQueryValue;
    char* firstCookieName;
    char* firstCookieValue;
    const char* paramValueView = NULL;
    size_t paramValueLen = 0;
    int ok;

    if ( state ) {
        state->getCount++;
    }

    ok =
        xrtWebRequestParam(req, "id", id, sizeof(id), NULL) &&
        xrtWebRequestQueryValue(req, "name", name, sizeof(name), NULL) &&
        strcmp(id, "42") == 0 &&
        strcmp(name, "xlang") == 0;
    idCopy = xrtWebRequestParamCopy(req, "id");
    nameCopy = xrtWebRequestQueryValueCopy(req, "name");
    cookieCopy = xrtWebRequestCookieCopy(req, "sid");
    firstHeaderName = xrtWebRequestHeaderNameCopyAt(req, 0);
    firstHeaderValue = xrtWebRequestHeaderValueCopyAt(req, 0);
    firstParamName = xrtWebRequestParamNameCopyAt(req, 0);
    firstParamValue = xrtWebRequestParamValueCopyAt(req, 0);
    firstQueryName = xrtWebRequestQueryNameCopyAt(req, 0);
    firstQueryValue = xrtWebRequestQueryValueCopyAt(req, 0);
    firstCookieName = xrtWebRequestCookieNameCopyAt(req, 0);
    firstCookieValue = xrtWebRequestCookieValueCopyAt(req, 0);
    ok = ok &&
         strcmp(idCopy, "42") == 0 &&
         strcmp(nameCopy, "xlang") == 0 &&
         strcmp(cookieCopy, "abc") == 0 &&
         xrtWebRequestHeaderCount(req) >= 2 &&
         firstHeaderName[0] != '\0' &&
         firstHeaderValue[0] != '\0' &&
         xrtWebRequestParamCount(req) == 1 &&
         strcmp(firstParamName, "id") == 0 &&
         strcmp(firstParamValue, "42") == 0 &&
         xrtWebRequestParamValueViewAt(req, 0, &paramValueView, &paramValueLen) &&
         paramValueView != NULL &&
         paramValueLen == 2 &&
         memcmp(paramValueView, "42", 2) == 0 &&
         xrtWebRequestQueryCount(req) == 1 &&
         xrtWebRequestQueryHasValueAt(req, 0) &&
         strcmp(firstQueryName, "name") == 0 &&
         strcmp(firstQueryValue, "xlang") == 0 &&
         xrtWebRequestCookieCount(req) == 1 &&
         xrtWebRequestConnectionOpen(req) &&
         strcmp(firstCookieName, "sid") == 0 &&
         strcmp(firstCookieValue, "abc") == 0;
    xrtFree(idCopy);
    xrtFree(nameCopy);
    xrtFree(cookieCopy);
    xrtFree(firstHeaderName);
    xrtFree(firstHeaderValue);
    xrtFree(firstParamName);
    xrtFree(firstParamValue);
    xrtFree(firstQueryName);
    xrtFree(firstQueryValue);
    xrtFree(firstCookieName);
    xrtFree(firstCookieValue);

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

static xwebaction handle_form(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    (void)userData;
    if ( xrtWebRequestIsFormUrlEncoded(req) &&
         xrtWebRequestFormCount(req) == 2 &&
         xrtWebRequestFormHasValueAt(req, 0) ) {
        char* name = xrtWebRequestFormValueCopy(req, "name");
        char* city = xrtWebRequestFormValueCopy(req, "city");
        char* firstName = xrtWebRequestFormNameCopyAt(req, 0);
        char* firstValue = xrtWebRequestFormValueCopyAt(req, 0);
        int ok =
            strcmp(name, "alice bob") == 0 &&
            strcmp(city, "shanghai") == 0 &&
            strcmp(firstName, "name") == 0 &&
            strcmp(firstValue, "alice bob") == 0;
        xrtFree(name);
        xrtFree(city);
        xrtFree(firstName);
        xrtFree(firstValue);
        return xrtWebResponseText(resp, ok ? "form-ok" : "form-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
    }
    return xrtWebResponseText(resp, "form-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_multipart(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    char* title;
    char* fieldName;
    char* uploadName;
    char* fileName;
    char* contentType;
    const void* fileData = NULL;
    size_t fileSize = 0;
    int ok;

    (void)userData;
    title = xrtWebRequestMultipartValueCopy(req, "title");
    fieldName = xrtWebRequestMultipartNameCopyAt(req, 0);
    uploadName = xrtWebRequestMultipartNameCopyAt(req, 1);
    fileName = xrtWebRequestMultipartFileNameCopyAt(req, 1);
    contentType = xrtWebRequestMultipartContentTypeCopyAt(req, 1);
    ok =
        xrtWebRequestIsMultipartForm(req) &&
        xrtWebRequestMultipartCount(req) == 2 &&
        !xrtWebRequestMultipartIsFileAt(req, 0) &&
        xrtWebRequestMultipartIsFileAt(req, 1) &&
        xrtWebRequestMultipartBodyViewAt(req, 1, &fileData, &fileSize) &&
        strcmp(title, "hello multipart") == 0 &&
        strcmp(fieldName, "title") == 0 &&
        strcmp(uploadName, "upload") == 0 &&
        strcmp(fileName, "note.txt") == 0 &&
        strcmp(contentType, "text/plain") == 0 &&
        fileSize == 9 &&
        fileData != NULL &&
        memcmp(fileData, "file-data", 9) == 0;
    xrtFree(title);
    xrtFree(fieldName);
    xrtFree(uploadName);
    xrtFree(fileName);
    xrtFree(contentType);
    return xrtWebResponseText(resp, ok ? "multipart-ok" : "multipart-bad", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static bool stream_begin(xwebrequest* req, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    (void)req;
    if ( !state ) {
        return false;
    }
    state->streamBody[0] = '\0';
    state->streamBodyLen = 0;
    return true;
}

static bool stream_chunk(xwebrequest* req, const void* data, size_t len, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    size_t room;
    (void)req;
    if ( !state || (!data && len > 0) ) {
        return false;
    }
    room = sizeof(state->streamBody) - state->streamBodyLen - 1;
    if ( len > room ) {
        return false;
    }
    if ( len > 0 ) {
        memcpy(state->streamBody + state->streamBodyLen, data, len);
        state->streamBodyLen += len;
        state->streamBody[state->streamBodyLen] = '\0';
    }
    return true;
}

static xwebaction stream_end(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    char* name;
    char text[384];
    if ( !state ) {
        return XWEB_ERROR;
    }
    state->streamCount++;
    name = xrtWebRequestParamCopy(req, "name");
    snprintf(text, sizeof(text), "stream:%s:%s", name ? name : "", state->streamBody);
    xrtFree(name);
    return xrtWebResponseText(resp, text, "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_any(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    HttpServerExampleState* state = (HttpServerExampleState*)userData;
    if ( state ) {
        state->anyCount++;
    }
    return xrtWebResponseText(resp, xrtWebRequestMethod(req), "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_cookie(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    (void)req;
    (void)userData;
    if ( !xrtWebResponseCookie(resp, "sid", "example", "/", 3600, XRT_SAME_SITE_LAX, XRT_SET_COOKIE_F_HTTP_ONLY) ) {
        return XWEB_ERROR;
    }
    return xrtWebResponseText(resp, "cookie set", "text/plain; charset=utf-8") ? XWEB_DONE : XWEB_ERROR;
}

static xwebaction handle_response_meta(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    const void* body;
    size_t bodyLen = 0;
    size_t headerCount;
    (void)req;
    (void)userData;

    xrtWebResponseStatus(resp, 200, "Metadata");
    if ( !xrtWebResponseSetHeader(resp, "Content-Type", "text/plain; charset=utf-8") ||
         !xrtWebResponseAddHeader(resp, "X-Duplicate", "first") ||
         !xrtWebResponseAddHeader(resp, "X-Duplicate", "second") ||
         !xrtWebResponseAppendText(resp, "meta-body") ) {
        return XWEB_ERROR;
    }
    headerCount = xrtWebResponseHeaderCount(resp);
    body = xrtWebResponseBodyView(resp, &bodyLen);
    if ( xrtWebResponseStatusCode(resp) != 200 ||
         strcmp(xrtWebResponseReason(resp), "Metadata") != 0 ||
         strcmp(xrtWebResponseHeader(resp, "X-Duplicate"), "first") != 0 ||
         headerCount < 3 ||
         xrtWebResponseHeaderNameAt(resp, headerCount) != NULL ||
         xrtWebResponseHeaderValueAt(resp, headerCount) != NULL ||
         bodyLen != 9 || !body || memcmp(body, "meta-body", 9) != 0 ||
         xrtWebResponseCommitted(resp) ) {
        return XWEB_ERROR;
    }
    return XWEB_DONE;
}

static xwebaction handle_file_range(xwebrequest* req, xwebresponse* resp, ptr userData)
{
    (void)req;
    xrtWebResponseStatus(resp, 206, "Partial Content");
    return xrtWebResponseFileRange(resp, (const char*)userData, 1, 4, 4096) ? XWEB_DONE : XWEB_ERROR;
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
    (void)xrtHttpRequestSetHeader(&req, "Cookie", "sid=abc");
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

static xhttpresponse* request_text_type(const char* url, const char* method, const char* body, const char* contentType)
{
    xhttprequest req;
    xhttpresponse* resp;
    xnet_result status = XRT_NET_ERROR;

    xrtHttpRequestInit(&req);
    (void)xrtHttpRequestSetURL(&req, url);
    if ( method ) {
        (void)xrtHttpRequestSetMethod(&req, method);
    }
    (void)xrtHttpRequestSetHeader(&req, "Cookie", "sid=abc");
    if ( body ) {
        (void)xrtHttpRequestSetBodyCopy(&req, body, strlen(body), contentType ? contentType : "text/plain");
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

static void require_header_response(const char* name, xhttpresponse* resp, unsigned code, const char* body, const char* headerName, const char* headerPart)
{
    const char* header = resp ? xrtHttpResponseHeader(resp, headerName) : NULL;
    require_line(name,
        resp != NULL &&
        resp->iStatusCode == code &&
        resp->pBody != NULL &&
        strstr(resp->pBody, body) != NULL &&
        header != NULL &&
        strstr(header, headerPart) != NULL);
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

    xrtDirCreateAll("examples_out/example_http_server_c_static");
    xrtFileWriteAll("examples_out/example_http_server_c_static/index.txt", "static-ok", 9, XRT_CP_UTF8);

    xrtWebConfigInit(&cfg);
    (void)xrtNetAddrParse(&cfg.tBindAddr, "127.0.0.1", 0);
    server = xrtWebServerCreate(NULL, &cfg);
    require_line("create", server != NULL);

    require_line("route_get", xrtWebServerGet(server, "/users/{id}", handle_user, &state));
    require_line("route_post", xrtWebServerPost(server, "/demo", handle_echo, &state));
    require_line("route_form", xrtWebServerPost(server, "/form", handle_form, &state));
    require_line("route_multipart", xrtWebServerPost(server, "/multipart", handle_multipart, &state));
    require_line("route_stream", xrtWebServerStreamRoute(server, XWEB_METHOD_POST, "/stream/{name}", stream_begin, stream_chunk, stream_end, &state));
    require_line("route_any", xrtWebServerAny(server, "/any", handle_any, &state));
    require_line("route_cookie", xrtWebServerGet(server, "/cookie", handle_cookie, &state));
    require_line("route_response_meta", xrtWebServerGet(server, "/response-meta", handle_response_meta, &state));
    require_line("route_file_range", xrtWebServerGet(server, "/file-range", handle_file_range, "examples_out/example_http_server_c_static/index.txt"));

    xrtWebStaticConfigInit(&staticCfg);
    require_line("static_mount", xrtWebServerStatic(server, "/public", "examples_out/example_http_server_c_static", &staticCfg));
    require_line("start", xrtWebServerStart(server) == XRT_NET_OK);

    port = xrtWebServerPort(server);
    require_line("port", port > 0);

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/users/42?name=xlang", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("get_response", resp, 200, "route-get-ok");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/demo?x=1", (unsigned)port);
    resp = request_text(url, "POST", "ping");
    require_response("post_response", resp, 200, "xlang-http-ok");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/form", (unsigned)port);
    resp = request_text_type(url, "POST", "name=alice+bob&city=shanghai", "application/x-www-form-urlencoded; charset=UTF-8");
    require_response("form_response", resp, 200, "form-ok");

    {
        const char* multipartBody =
            "--xlang\r\n"
            "Content-Disposition: form-data; name=\"title\"\r\n"
            "\r\n"
            "hello multipart\r\n"
            "--xlang\r\n"
            "Content-Disposition: form-data; name=\"upload\"; filename=\"note.txt\"\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "file-data\r\n"
            "--xlang--\r\n";
        snprintf(url, sizeof(url), "http://127.0.0.1:%u/multipart", (unsigned)port);
        resp = request_text_type(url, "POST", multipartBody, "multipart/form-data; boundary=xlang");
        require_response("multipart_response", resp, 200, "multipart-ok");
    }

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/stream/upload", (unsigned)port);
    resp = request_text(url, "POST", "hello-world");
    require_response("stream_response", resp, 200, "stream:upload:hello-world");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/any", (unsigned)port);
    resp = request_text(url, "PUT", NULL);
    require_response("any_response", resp, 200, "PUT");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/cookie", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_header_response("cookie_response", resp, 200, "cookie set", "Set-Cookie", "sid=example; Path=/; Max-Age=3600; SameSite=Lax; HttpOnly");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/response-meta", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("response_meta", resp, 200, "meta-body");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/file-range", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("file_range", resp, 206, "tati");

    snprintf(url, sizeof(url), "http://127.0.0.1:%u/public/index.txt", (unsigned)port);
    resp = request_text(url, "GET", NULL);
    require_response("static_response", resp, 200, "static-ok");

    require_line("callback_count", state.getCount == 1 && state.postCount == 1 && state.anyCount == 1 && state.streamCount == 1);

    xrtHttpCloseIdleConnections(xrtNetSyncGetHiddenEngine());
    xrtNetSyncShutdownHiddenEngine();
    xrtWebServerDestroy(server);
    xrtFileDelete("examples_out/example_http_server_c_static/index.txt");
    xrtDirDelete("examples_out/example_http_server_c_static");
    xrtUnit();
    return 0;
}
