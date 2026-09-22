static const char xl6HttpServerResponseSource[] = XL6_HTTP_SERVER_SOURCE(
static bool xl6_http_response_init(XL6_Http_Response* response, XL6_Http_Request* request, XL6_Http_Server* owner, xhttpconn* connection)
{
    memset(response, 0, sizeof(*response));
    response->Owner = owner;
    response->Request = request;
    response->Connection = connection;
    response->Reply = xrtHttpReplyCreate(200u);
    return response->Reply != 0 && xrtHttpReplySetHeader(response->Reply, xl6_http_view("X-Powered-By"), xl6_http_view("xlang"));
}

static void xl6_http_response_unit(XL6_Http_Response* response)
{
    if ( response == 0 ) { return; }
    xrtBufferDestroy(response->Buffer);
    xrtHttpReplyDestroy(response->Reply);
    memset(response, 0, sizeof(*response));
}

static bool xl6_http_response_buffer(XL6_Http_Response* response)
{
    if ( response == 0 || response->Committed ) { return false; }
    if ( response->Buffer == 0 ) { response->Buffer = xrtBufferCreate(); }
    return response->Buffer != 0;
}

static bool xl6_http_response_set_body(XL6_Http_Response* response, const void* data, size_t size, const char* contentType)
{
    if ( (size != 0u && data == 0) || !xl6_http_response_buffer(response) ) { return false; }
    if ( !xrtBufferResize(response->Buffer, size) ) { return false; }
    if ( size != 0u ) { memcpy(response->Buffer->Data, data, size); }
    if ( contentType != 0 && contentType[0] != 0 && !xrtHttpReplySetHeader(response->Reply, xl6_http_view("Content-Type"), xl6_http_view(contentType)) ) { return false; }
    return true;
}

static bool xl6_http_response_append(XL6_Http_Response* response, const void* data, size_t size)
{
    size_t oldSize;
    if ( (size != 0u && data == 0) || !xl6_http_response_buffer(response) ) { return false; }
    oldSize = response->Buffer->Size;
    if ( size > SIZE_MAX - oldSize || !xrtBufferResize(response->Buffer, oldSize + size) ) { return false; }
    if ( size != 0u ) { memcpy(response->Buffer->Data + oldSize, data, size); }
    return true;
}

static bool xl6_http_response_finish(XL6_Http_Response* response)
{
    if ( response != 0 && response->Request != 0 && response->Request->ErrorStatus != 0u ) {
        uint16_t status = response->Request->ErrorStatus;
        const char* text = status == 413u ? "Content Too Large" : "Bad Request";
        xhttpreply* reply = xrtHttpReplyCreate(status);
        if ( reply == 0 ) { return false; }
        xrtHttpReplyDestroy(response->Reply);
        response->Reply = reply;
        if ( response->Buffer != 0 ) { xrtBufferClear(response->Buffer); }
        if ( !xrtHttpReplySetBytes(reply, xl6_http_bytes(text, strlen(text)), xl6_http_view("text/plain; charset=utf-8")) ) { return false; }
        response->Request = 0;
    }
    if ( response == 0 || response->Committed || response->Reply == 0 || response->Connection == 0 ) { return false; }
    if ( response->Buffer != 0 && !xrtHttpReplySetBytes(response->Reply, xl6_http_bytes(response->Buffer->Data, response->Buffer->Size), xl6_http_view(0)) ) { return false; }
    if ( xrtHttpConnRespond(response->Connection, response->Reply) != 0 ) { return false; }
    response->Committed = true;
    return true;
}

void* xl6_xrt_http_response_wrap(void* handle) { return handle; }

void xl6_xrt_http_response_status(void* raw, long long code, const char* reason)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    if ( response == 0 || response->Committed || response->Reply == 0 || code < 100 || code > 999 ) { return; }
    if ( !xrtHttpReplySetStatus(response->Reply, (uint16_t)code) ) { return; }
    if ( reason != 0 && reason[0] != 0 ) { (void)xrtHttpReplySetReason(response->Reply, xl6_http_view(reason)); }
}

int xl6_xrt_http_response_header(void* raw, const char* name, const char* value)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && !response->Committed && response->Reply != 0 && name != 0 && value != 0 && xrtHttpReplySetHeader(response->Reply, xl6_http_view(name), xl6_http_view(value));
}

int xl6_xrt_http_response_add_header(void* raw, const char* name, const char* value)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && !response->Committed && response->Reply != 0 && name != 0 && value != 0 && xrtHttpReplyAddHeader(response->Reply, xl6_http_view(name), xl6_http_view(value));
}

long long xl6_xrt_http_response_status_code(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && response->Reply != 0 ? (long long)xrtHttpReplyStatus(response->Reply) : 0;
}

void* xl6_xrt_http_response_reason(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return xl6_http_copy_view(response != 0 && response->Reply != 0 ? xrtHttpReplyReason(response->Reply) : xl6_http_view(0));
}

void* xl6_xrt_http_response_header_value(void* raw, const char* name)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    const xhttpfield* field = response != 0 && response->Reply != 0 && name != 0 ? xrtHttpReplyHeader(response->Reply, xl6_http_view(name)) : 0;
    return xl6_http_copy_view(field != 0 ? field->Value : xl6_http_view(0));
}

long long xl6_xrt_http_response_header_count(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && response->Reply != 0 ? (long long)xrtHttpReplyHeaderCount(response->Reply) : 0;
}

void* xl6_xrt_http_response_header_name(void* raw, long long index)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    const xhttpfield* field = index >= 0 && response != 0 && response->Reply != 0 ? xrtHttpReplyHeaderAt(response->Reply, (size_t)index) : 0;
    return xl6_http_copy_view(field != 0 ? field->Name : xl6_http_view(0));
}

void* xl6_xrt_http_response_header_value_at(void* raw, long long index)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    const xhttpfield* field = index >= 0 && response != 0 && response->Reply != 0 ? xrtHttpReplyHeaderAt(response->Reply, (size_t)index) : 0;
    return xl6_http_copy_view(field != 0 ? field->Value : xl6_http_view(0));
}

long long xl6_xrt_http_response_body_size(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && response->Buffer != 0 ? (long long)response->Buffer->Size : 0;
}

void* xl6_xrt_http_response_body_buffer(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return xl6_xrt_buffer_from_view_bytes(response != 0 && response->Buffer != 0 ? response->Buffer->Data : 0, response != 0 && response->Buffer != 0 ? response->Buffer->Size : 0u);
}

int xl6_xrt_http_response_committed(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return response != 0 && response->Committed;
}

static bool xl6_http_response_cookie_set(XL6_Http_Response* response, const char* name, const char* value, const char* path, long long maxAge, long long sameSite, uint32_t flags)
{
    xsetcookie cookie;
    char* text;
    size_t size = 0u;
    bool result;
    if ( response == 0 || response->Committed || response->Reply == 0 || name == 0 || value == 0 || sameSite < 0 || sameSite > 3 ) { return false; }
    memset(&cookie, 0, sizeof(cookie));
    cookie.Name = xl6_http_view(name);
    cookie.Value = xl6_http_view(value);
    cookie.Flags = flags & (UINT32_C(0x20) | UINT32_C(0x40) | UINT32_C(0x80));
    if ( path != 0 ) { cookie.Path = xl6_http_view(path); cookie.Flags |= UINT32_C(0x02); }
    if ( maxAge >= 0 ) { cookie.MaxAge = maxAge; cookie.Flags |= UINT32_C(0x08); }
    if ( sameSite != 0 ) { cookie.SameSite = (int)sameSite; cookie.Flags |= UINT32_C(0x10); }
    text = xrtSetCookieBuild(&cookie, &size);
    if ( text == 0 ) { return false; }
    result = xrtHttpReplyAddHeader(response->Reply, xl6_http_view("Set-Cookie"), (xstrview){ text, size });
    xrtFree(text);
    return result;
}

int xl6_xrt_http_response_cookie(void* raw, const char* name, const char* value, const char* path, long long maxAge, long long sameSite, uint32_t flags)
{
    return xl6_http_response_cookie_set((XL6_Http_Response*)raw, name, value, path, maxAge, sameSite, flags);
}

int xl6_xrt_http_response_delete_cookie(void* raw, const char* name, const char* path)
{
    return xl6_http_response_cookie_set((XL6_Http_Response*)raw, name, "", path, 0, 0, 0u);
}

int xl6_xrt_http_response_text(void* raw, const char* text, const char* contentType)
{
    return text != 0 && xl6_http_response_set_body((XL6_Http_Response*)raw, text, strlen(text), contentType);
}

int xl6_xrt_http_response_json_text(void* raw, const char* text)
{
    return text != 0 && xl6_http_response_set_body((XL6_Http_Response*)raw, text, strlen(text), "application/json; charset=utf-8");
}

int xl6_xrt_http_response_body(void* raw, void* data, size_t size, const char* contentType)
{
    return xl6_http_response_set_body((XL6_Http_Response*)raw, data, size, contentType);
}

int xl6_xrt_http_response_reserve_body(void* raw, size_t size)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    return xl6_http_response_buffer(response) && xrtBufferReserve(response->Buffer, size);
}

int xl6_xrt_http_response_append_body(void* raw, void* data, size_t size)
{
    return xl6_http_response_append((XL6_Http_Response*)raw, data, size);
}

int xl6_xrt_http_response_append_text(void* raw, const char* text)
{
    return text != 0 && xl6_http_response_append((XL6_Http_Response*)raw, text, strlen(text));
}

int xl6_xrt_http_response_start(void* raw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    if ( !xl6_http_response_buffer(response) ) { return false; }
    response->Started = true;
    return true;
}

int xl6_xrt_http_response_send(void* raw, void* data, size_t size) { return xl6_http_response_append((XL6_Http_Response*)raw, data, size); }
int xl6_xrt_http_response_write(void* raw, const char* text) { return text != 0 && xl6_http_response_append((XL6_Http_Response*)raw, text, strlen(text)); }
int xl6_xrt_http_response_end(void* raw) { return xl6_http_response_finish((XL6_Http_Response*)raw); }

int xl6_xrt_http_response_file(void* raw, const char* path, size_t chunkSize)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    xstrview contentType;
    (void)chunkSize;
    if ( response == 0 || response->Committed || response->Owner == 0 || response->Owner->Pool == 0 || path == 0 ) { return false; }
    if ( response->Request != 0 && response->Request->ErrorStatus != 0u ) { return xl6_http_response_finish(response); }
    contentType = xrtMimeByPath(xl6_http_view(path));
    if ( !xrtHttpConnFile(response->Connection, response->Owner->Pool, xrtHttpReplyStatus(response->Reply), contentType, path) ) { return false; }
    response->Committed = true;
    return true;
}

int xl6_xrt_http_response_file_range(void* raw, const char* path, long long offset, long long length, size_t chunkSize)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    xstrview contentType;
    (void)chunkSize;
    if ( response == 0 || response->Committed || response->Owner == 0 || response->Owner->Pool == 0 || path == 0 || offset < 0 || length < 0 ) { return false; }
    if ( response->Request != 0 && response->Request->ErrorStatus != 0u ) { return xl6_http_response_finish(response); }
    contentType = xrtMimeByPath(xl6_http_view(path));
    if ( !xrtHttpConnFileRange(response->Connection, response->Owner->Pool, xrtHttpReplyStatus(response->Reply), contentType, path, (uint64_t)offset, (uint64_t)length) ) { return false; }
    response->Committed = true;
    return true;
}

int xl6_xrt_http_response_redirect(void* raw, const char* url, long long code)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    if ( response == 0 || response->Committed || response->Reply == 0 || url == 0 || code < 300 || code > 399 ) { return false; }
    return xrtHttpReplySetStatus(response->Reply, (uint16_t)code) && xrtHttpReplySetHeader(response->Reply, xl6_http_view("Location"), xl6_http_view(url)) && xl6_http_response_set_body(response, "", 0u, "text/plain; charset=utf-8");
}

void* xl6_xrt_http_mime_by_path(const char* path)
{
    return xl6_http_copy_view(xrtMimeByPath(xl6_http_view(path)));
}

void* xl6_xrt_http_body_chunk_text(void* data, size_t size)
{
    return xl6_xrt_string_dup((const char*)(data != 0 ? data : ""), data != 0 ? size : 0u);
}

void* xl6_xrt_http_body_chunk_bytes(void* data, size_t size)
{
    return xl6_xrt_buffer_from_view_bytes(data, data != 0 ? size : 0u);
}
    ) "\n";



/* Router 回调把 xlang 手感层收敛到 XRT 的唯一最终响应门。 */
static const char xl6HttpServerDispatchSource[] = XL6_HTTP_SERVER_SOURCE(
static void xl6_http_route_fail(xhttpconn* connection)
{
    (void)xrtHttpConnReply(connection, 500u, xl6_http_view("text/plain; charset=utf-8"), xl6_http_bytes("", 0u));
}

static bool xl6_http_call_bool(XL6_Function function, int count, xvalue* first, xvalue* second, xvalue* third)
{
    xvalue* result;
    bool ok;
    if ( count == 1 ) { result = xl6_function_call_var(function, 1, first); }
    else if ( count == 2 ) { result = xl6_function_call_var(function, 2, first, second); }
    else { result = xl6_function_call_var(function, 3, first, second, third); }
    ok = result != 0 && xl6_xrt_value_bool(result);
    if ( result != 0 ) { xrtValueRelease(result); }
    return ok;
}

static void xl6_http_route_request(xhttpserver* nativeServer, xhttpconn* connection, const xhttpserverrequest* nativeRequest, const xhttprouteparam* params, size_t paramCount, void* data)
{
    XL6_Http_Route* route = (XL6_Http_Route*)data;
    XL6_Http_Request request;
    XL6_Http_Response response;
    xvalue* result = 0;
    bool ok = false;
    (void)nativeServer;
    if ( route == 0 || connection == 0 || nativeRequest == 0 ) { xl6_http_route_fail(connection); return; }
    if ( route->Kind == XL6_HTTP_ROUTE_STATIC ) {
        if ( route->Owner != 0 && route->Owner->Pool != 0 && route->Static != 0 && xrtHttpConnStatic(connection, route->Owner->Pool, route->Static->Root, &route->Static->Config) ) { return; }
        (void)xrtHttpConnReply(connection, 503u, xl6_http_view("text/plain; charset=utf-8"), xl6_http_bytes("Service Unavailable", 19u));
        return;
    }
    xl6_http_request_init(&request, nativeRequest, connection, params, paramCount, route->Owner);
    if ( route->Kind != XL6_HTTP_ROUTE_STREAM && xl6_http_request_media_is(&request, "multipart", "form-data") ) {
        (void)xl6_http_request_form_data(&request);
    }
    if ( route->Kind == XL6_HTTP_ROUTE_TEXT && xl6_http_function_valid(&route->Handler) ) {
        xbytesview body = xrtHttpServerRequestBody(nativeRequest);
        xstrview path = xl6_http_request_target_ready(&request) ? request.Target.Uri.Path : xl6_http_view(0);
        xstrview query = xl6_http_request_target_ready(&request) ? request.Target.Uri.Query : xl6_http_view(0);
        result = xl6_function_call_var(route->Handler, 4, xl6_http_text_value(xrtHttpServerRequestMethod(nativeRequest)), xl6_http_text_value(path), xl6_http_text_value(query), xl6_http_text_value((xstrview){ (const char*)body.Data, body.Size }));
        if ( xl6_http_response_init(&response, &request, route->Owner, connection) ) {
            const char* text = result != 0 ? xl6_xrt_value_text(result) : "";
            if ( text == 0 ) { text = ""; }
            ok = xl6_http_response_set_body(&response, text, strlen(text), "text/plain; charset=utf-8") && xl6_http_response_finish(&response);
            xl6_http_response_unit(&response);
        }
    } else if ( (route->Kind == XL6_HTTP_ROUTE_OBJECT || route->Kind == XL6_HTTP_ROUTE_STREAM) && xl6_http_function_valid(route->Kind == XL6_HTTP_ROUTE_STREAM ? &route->EndHandler : &route->Handler) ) {
        XL6_Function function = route->Kind == XL6_HTTP_ROUTE_STREAM ? route->EndHandler : route->Handler;
        if ( xl6_http_response_init(&response, &request, route->Owner, connection) ) {
            ok = xl6_http_call_bool(function, 2, xrtValuePointer(&request), xrtValuePointer(&response), 0);
            if ( ok && !response.Committed ) { ok = xl6_http_response_finish(&response); }
            if ( !ok && !response.Committed ) {
                (void)xrtHttpReplySetStatus(response.Reply, 500u);
                (void)xl6_http_response_set_body(&response, "", 0u, "text/plain; charset=utf-8");
                (void)xl6_http_response_finish(&response);
            }
            xl6_http_response_unit(&response);
        }
    } else if ( route->Kind == XL6_HTTP_ROUTE_ERROR && xl6_http_function_valid(&route->Handler) ) {
        if ( xl6_http_response_init(&response, &request, route->Owner, connection) ) {
            result = xl6_function_call_var(route->Handler, 4, xrtValueInt(404), xl6_http_text_value(xl6_http_view("Not Found")), xrtValuePointer(&request), xrtValuePointer(&response));
            ok = result != 0 && xl6_xrt_value_bool(result);
            if ( ok && !response.Committed ) { ok = xl6_http_response_finish(&response); }
            if ( !ok && !response.Committed ) { (void)xrtHttpReplySetStatus(response.Reply, 404u); (void)xl6_http_response_finish(&response); }
            xl6_http_response_unit(&response);
        }
    }
    if ( result != 0 ) { xrtValueRelease(result); }
    xl6_http_request_unit(&request);
    if ( !ok && route->Kind == XL6_HTTP_ROUTE_TEXT ) { xl6_http_route_fail(connection); }
}

static xhttpserverbodypolicy xl6_http_route_headers(xhttpserver* nativeServer, xhttpconn* connection, const xhttpserverrequest* nativeRequest, const xhttprouteparam* params, size_t paramCount, void* data)
{
    XL6_Http_Route* route = (XL6_Http_Route*)data;
    XL6_Http_Request request;
    bool ok;
    (void)nativeServer;
    if ( route == 0 || !xl6_http_function_valid(&route->BeginHandler) ) { return XHTTP_SERVER_BODY_REJECT; }
    xl6_http_request_init(&request, nativeRequest, connection, params, paramCount, route->Owner);
    ok = xl6_http_call_bool(route->BeginHandler, 1, xrtValuePointer(&request), 0, 0);
    xl6_http_request_unit(&request);
    return ok ? XHTTP_SERVER_BODY_STREAM : XHTTP_SERVER_BODY_REJECT;
}

static bool xl6_http_route_body(xhttpserver* nativeServer, xhttpconn* connection, const xhttpserverrequest* nativeRequest, const xhttprouteparam* params, size_t paramCount, xbytesview body, void* data)
{
    XL6_Http_Route* route = (XL6_Http_Route*)data;
    XL6_Http_Request request;
    bool ok;
    (void)nativeServer;
    if ( route == 0 || !xl6_http_function_valid(&route->ChunkHandler) ) { return false; }
    xl6_http_request_init(&request, nativeRequest, connection, params, paramCount, route->Owner);
    ok = xl6_http_call_bool(route->ChunkHandler, 3, xrtValuePointer(&request), xrtValuePointer((void*)body.Data), xrtValueInt((long long)body.Size));
    xl6_http_request_unit(&request);
    return ok;
}

static bool xl6_http_router_add_view(XL6_Http_Router* shared, xstrview method, const char* pattern, XL6_Http_Route* route)
{
    bool added;
    xhttpserverrouteevents events;
    if ( shared == 0 || shared->Router == 0 || pattern == 0 || route == 0 || xrtHttpServerRouterFrozen(shared->Router) ) { return false; }
    xrtHttpServerRouteEventsInit(&events);
    if ( route->Kind == XL6_HTTP_ROUTE_STREAM ) {
        events.Headers = xl6_http_route_headers;
        events.Body = xl6_http_route_body;
        events.Request = xl6_http_route_request;
    } else {
        events.Request = xl6_http_route_request;
    }
    events.Release = xl6_http_route_release;
    events.Data = route;
    added = xrtHttpServerRouteEvents(shared->Router, method, xl6_http_view(pattern), &events);
    if ( !added ) { return false; }
    xl6_http_router_add_owned(shared, route);
    return true;
}

static bool xl6_http_router_add(XL6_Http_Router* shared, const char* method, const char* pattern, XL6_Http_Route* route)
{
    size_t offset = 0u;
    xstrview methodView;
    if ( !xl6_http_methods_valid(method) || !xl6_http_method_part(method, &offset, &methodView) || offset != SIZE_MAX ) { return false; }
    return xl6_http_router_add_view(shared, methodView, pattern, route);
}

static bool xl6_http_add_handler(XL6_Http_Router* shared, const char* method, const char* pattern, XL6_Function handler, int kind)
{
    size_t offset = 0u;
    xstrview methodView;
    XL6_Http_Route* route;
    if ( !xl6_http_function_valid(&handler) || !xl6_http_methods_valid(method) ) { return false; }
    do {
        if ( !xl6_http_method_part(method, &offset, &methodView) ) { return false; }
        route = xl6_http_route_create(kind);
        if ( route == 0 ) { return false; }
        route->Handler = handler;
        xl6_http_retain_function(&route->Handler);
        if ( !xl6_http_router_add_view(shared, methodView, pattern, route) ) { xl6_http_route_release(route); return false; }
    } while ( offset != SIZE_MAX );
    return true;
}

static bool xl6_http_add_stream(XL6_Http_Router* shared, const char* method, const char* pattern, XL6_Function beginHandler, XL6_Function chunkHandler, XL6_Function endHandler)
{
    size_t offset = 0u;
    xstrview methodView;
    XL6_Http_Route* route;
    if ( !xl6_http_function_valid(&beginHandler) || !xl6_http_function_valid(&chunkHandler) || !xl6_http_function_valid(&endHandler) || !xl6_http_methods_valid(method) ) { return false; }
    do {
        if ( !xl6_http_method_part(method, &offset, &methodView) ) { return false; }
        route = xl6_http_route_create(XL6_HTTP_ROUTE_STREAM);
        if ( route == 0 ) { return false; }
        route->BeginHandler = beginHandler;
        route->ChunkHandler = chunkHandler;
        route->EndHandler = endHandler;
        xl6_http_retain_function(&route->BeginHandler);
        xl6_http_retain_function(&route->ChunkHandler);
        xl6_http_retain_function(&route->EndHandler);
        if ( !xl6_http_router_add_view(shared, methodView, pattern, route) ) { xl6_http_route_release(route); return false; }
    } while ( offset != SIZE_MAX );
    return true;
}

static bool xl6_http_set_error(XL6_Http_Router* shared, XL6_Function handler)
{
    XL6_Http_Route* route;
    if ( shared == 0 || !xl6_http_function_valid(&handler) || xrtHttpServerRouterFrozen(shared->Router) ) { return false; }
    if ( shared->ErrorRoute != 0 ) {
        xl6_http_release_function(&shared->ErrorRoute->Handler);
        shared->ErrorRoute->Handler = handler;
        xl6_http_retain_function(&shared->ErrorRoute->Handler);
        return true;
    }
    route = xl6_http_route_create(XL6_HTTP_ROUTE_ERROR);
    if ( route == 0 ) { return false; }
    route->Handler = handler;
    xl6_http_retain_function(&route->Handler);
    if ( !xl6_http_router_add(shared, "ANY", "/{path...}", route) ) { xl6_http_route_release(route); return false; }
    shared->ErrorRoute = route;
    return true;
}
    ) "\n";



/* 静态服务直接复用 XRT 的安全 Root、条件请求、Range 与 MIME 体系。 */
