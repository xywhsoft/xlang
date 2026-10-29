static const char xl6HttpServerRequestSource[] = XL6_HTTP_SERVER_SOURCE(
static void xl6_http_request_init(XL6_Http_Request* request, const xhttpserverrequest* native, xhttpconn* connection, const xhttprouteparam* params, size_t paramCount, XL6_Http_Server* owner)
{
    memset(request, 0, sizeof(*request));
    request->Request = native;
    request->Connection = connection;
    request->Params = params;
    request->ParamCount = paramCount;
    request->Owner = owner;
}

static void xl6_http_request_unit(XL6_Http_Request* request)
{
    if ( request == 0 ) { return; }
    xrtQueryParamsDestroy(request->Query);
    xrtQueryParamsDestroy(request->Form);
    xrtFormDataDestroy(request->FormData);
    xrtFree(request->Cookies);
    memset(request, 0, sizeof(*request));
}

static bool xl6_http_request_target_ready(XL6_Http_Request* request)
{
    if ( request == 0 || request->Request == 0 ) { return false; }
    if ( !request->TargetTried ) {
        request->TargetTried = true;
        request->TargetReady = xrtHttpServerRequestParseTarget(request->Request, &request->Target);
    }
    return request->TargetReady;
}

static xqueryparams* xl6_http_request_query_params(XL6_Http_Request* request)
{
    size_t offset = 0u;
    if ( request == 0 || request->Request == 0 ) { return 0; }
    if ( !request->QueryTried ) {
        request->QueryTried = true;
        request->Query = xrtHttpServerRequestQueryParams(request->Request, 0, &offset);
    }
    return request->Query;
}

static xqueryparams* xl6_http_request_form_params(XL6_Http_Request* request)
{
    size_t offset = 0u;
    if ( request == 0 || request->Request == 0 ) { return 0; }
    if ( !request->FormTried ) {
        request->FormTried = true;
        request->Form = xrtHttpServerRequestForm(request->Request, 0, &offset);
    }
    return request->Form;
}

static xformdata* xl6_http_request_form_data(XL6_Http_Request* request)
{
    xformdataconfig config;
    xmultipartlimits limits;
    xmultiparterrorinfo error;
    size_t parts = request != 0 && request->Owner != 0 ? request->Owner->MultipartParts : 0u;
    size_t partBytes = request != 0 && request->Owner != 0 ? request->Owner->MultipartPartBytes : 0u;
    if ( request == 0 || request->Request == 0 ) { return 0; }
    if ( request->FormDataTried ) { return request->FormData; }
    request->FormDataTried = true;
    xrtFormDataConfigInit(&config);
    if ( parts != 0u ) {
        config.MaxParts = parts;
        if ( config.InitialParts > parts ) { config.InitialParts = parts; }
    }
    if ( partBytes != 0u ) { config.MaxPartBytes = partBytes; }
    xrtMultipartLimitsInit(&limits);
    if ( parts != 0u ) { limits.MaxParts = parts; }
    if ( partBytes != 0u ) { limits.MaxPartBytes = partBytes; }
    if ( request->Owner != 0 && request->Owner->Config.Http1.Body.MaxBody != UINT64_MAX && request->Owner->Config.Http1.Body.MaxBody <= SIZE_MAX ) {
        limits.MaxBodyBytes = (size_t)request->Owner->Config.Http1.Body.MaxBody;
    }
    memset(&error, 0, sizeof(error));
    request->FormData = xrtHttpServerRequestFormData(request->Request, &config, &limits, &error);
    if ( request->FormData == 0 && error.Code != 0 ) {
        request->ErrorStatus = error.Code >= 10 ? 413u : 400u;
    }
    return request->FormData;
}

static bool xl6_http_request_cookies(XL6_Http_Request* request)
{
    size_t count = 0u;
    if ( request == 0 || request->Request == 0 ) { return false; }
    if ( request->CookiesTried ) { return request->Cookies != 0 || request->CookieCount == 0u; }
    request->CookiesTried = true;
    if ( !xrtHttpServerRequestCookies(request->Request, 0, 0u, &count) ) { return false; }
    if ( count == 0u ) { return true; }
    if ( count > SIZE_MAX / sizeof(xcookiepair) ) { return false; }
    request->Cookies = (xcookiepair*)xrtMalloc(count * sizeof(xcookiepair));
    if ( request->Cookies == 0 ) { return false; }
    if ( !xrtHttpServerRequestCookies(request->Request, request->Cookies, count, &request->CookieCount) ) {
        xrtFree(request->Cookies);
        request->Cookies = 0;
        request->CookieCount = 0u;
        return false;
    }
    return true;
}

void* xl6_xrt_http_request_wrap(void* handle) { return handle; }

void* xl6_xrt_http_request_method(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(request != 0 && request->Request != 0 ? xrtHttpServerRequestMethod(request->Request) : xl6_http_view(0));
}

long long xl6_xrt_http_request_method_id(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return request != 0 && request->Request != 0 ? xl6_http_method_id_view(xrtHttpServerRequestMethod(request->Request)) : 0;
}

void* xl6_xrt_http_request_target(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(request != 0 && request->Request != 0 ? xrtHttpServerRequestTarget(request->Request) : xl6_http_view(0));
}

void* xl6_xrt_http_request_path(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(xl6_http_request_target_ready(request) ? request->Target.Uri.Path : xl6_http_view(0));
}

void* xl6_xrt_http_request_query(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(xl6_http_request_target_ready(request) ? request->Target.Uri.Query : xl6_http_view(0));
}

long long xl6_xrt_http_request_query_count(void* raw)
{
    xqueryparams* params = xl6_http_request_query_params((XL6_Http_Request*)raw);
    return params != 0 ? (long long)xrtQueryParamsCount(params) : 0;
}

int xl6_xrt_http_request_query_has_value(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* params = xl6_http_request_query_params((XL6_Http_Request*)raw);
    return index >= 0 && params != 0 && xrtQueryParamsAt(params, (size_t)index, &pair) && (pair.Flags & 1u) != 0u;
}

void* xl6_xrt_http_request_query_name(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* params = xl6_http_request_query_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(index >= 0 && params != 0 && xrtQueryParamsAt(params, (size_t)index, &pair) ? pair.Key : xl6_http_view(0));
}

void* xl6_xrt_http_request_query_value_at(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* params = xl6_http_request_query_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(index >= 0 && params != 0 && xrtQueryParamsAt(params, (size_t)index, &pair) ? pair.Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_query_value(void* raw, const char* name)
{
    xquerypair pair;
    xqueryparams* params = xl6_http_request_query_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(name != 0 && params != 0 && xrtQueryParamsGet(params, xl6_http_view(name), &pair) ? pair.Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_header(void* raw, const char* name)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    const xhttpfield* field = request != 0 && request->Request != 0 && name != 0 ? xrtHttpServerRequestHeader(request->Request, xl6_http_view(name)) : 0;
    return xl6_http_copy_view(field != 0 ? field->Value : xl6_http_view(0));
}

long long xl6_xrt_http_request_header_count(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return request != 0 && request->Request != 0 ? (long long)xrtHttpServerRequestHeaderCount(request->Request) : 0;
}

void* xl6_xrt_http_request_header_name(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    const xhttpfield* field = index >= 0 && request != 0 && request->Request != 0 ? xrtHttpServerRequestHeaderAt(request->Request, (size_t)index) : 0;
    return xl6_http_copy_view(field != 0 ? field->Name : xl6_http_view(0));
}

void* xl6_xrt_http_request_header_value(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    const xhttpfield* field = index >= 0 && request != 0 && request->Request != 0 ? xrtHttpServerRequestHeaderAt(request->Request, (size_t)index) : 0;
    return xl6_http_copy_view(field != 0 ? field->Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_param(void* raw, const char* name)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    size_t index;
    if ( request != 0 && name != 0 ) {
        for ( index = 0u; index < request->ParamCount; index++ ) {
            if ( xl6_http_view_equal_text(request->Params[index].Name, name) ) { return xl6_http_copy_view(request->Params[index].Value); }
        }
    }
    return xl6_http_copy_view(xl6_http_view(0));
}

long long xl6_xrt_http_request_param_count(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return request != 0 ? (long long)request->ParamCount : 0;
}

void* xl6_xrt_http_request_param_name(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(index >= 0 && request != 0 && (size_t)index < request->ParamCount ? request->Params[index].Name : xl6_http_view(0));
}

void* xl6_xrt_http_request_param_value(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(index >= 0 && request != 0 && (size_t)index < request->ParamCount ? request->Params[index].Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_cookie(void* raw, const char* name)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    size_t index;
    if ( name != 0 && xl6_http_request_cookies(request) ) {
        for ( index = 0u; index < request->CookieCount; index++ ) {
            if ( xl6_http_view_equal_text(request->Cookies[index].Name, name) ) { return xl6_http_copy_view(request->Cookies[index].Value); }
        }
    }
    return xl6_http_copy_view(xl6_http_view(0));
}

long long xl6_xrt_http_request_cookie_count(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_request_cookies(request) ? (long long)request->CookieCount : 0;
}

void* xl6_xrt_http_request_cookie_name(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(index >= 0 && xl6_http_request_cookies(request) && (size_t)index < request->CookieCount ? request->Cookies[index].Name : xl6_http_view(0));
}

void* xl6_xrt_http_request_cookie_value_at(void* raw, long long index)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return xl6_http_copy_view(index >= 0 && xl6_http_request_cookies(request) && (size_t)index < request->CookieCount ? request->Cookies[index].Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_body_text(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    xbytesview body = request != 0 && request->Request != 0 ? xrtHttpServerRequestBody(request->Request) : xl6_http_bytes(0, 0u);
    return xl6_xrt_string_dup((const char*)(body.Data != 0 ? body.Data : (const unsigned char*)""), body.Size);
}

void* xl6_xrt_http_request_body_data(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return (void*)(request != 0 && request->Request != 0 ? xrtHttpServerRequestBody(request->Request).Data : 0);
}

long long xl6_xrt_http_request_body_size(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return request != 0 && request->Request != 0 ? (long long)xrtHttpServerRequestBody(request->Request).Size : 0;
}

void* xl6_xrt_http_request_body_buffer(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    xbytesview body = request != 0 && request->Request != 0 ? xrtHttpServerRequestBody(request->Request) : xl6_http_bytes(0, 0u);
    return xl6_xrt_buffer_from_view_bytes(body.Data, body.Size);
}

long long xl6_xrt_http_request_content_length(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    const xhttpfield* field = request != 0 && request->Request != 0 ? xrtHttpServerRequestHeader(request->Request, xl6_http_view("Content-Length")) : 0;
    return field != 0 ? (long long)xrtHttpServerRequestContentLength(request->Request) : -1;
}

void* xl6_xrt_http_request_content_type(void* raw)
{
    return xl6_xrt_http_request_header(raw, "Content-Type");
}

static bool xl6_http_request_media_is(XL6_Http_Request* request, const char* type, const char* subtype)
{
    xmediatype media;
    return request != 0 && request->Request != 0 && xrtHttpServerRequestContentType(request->Request, &media) == 1 && xrtHttpMediaTypeEqual(&media, xl6_http_view(type), xl6_http_view(subtype));
}

int xl6_xrt_http_request_is_form_urlencoded(void* raw)
{
    return xl6_http_request_media_is((XL6_Http_Request*)raw, "application", "x-www-form-urlencoded");
}

long long xl6_xrt_http_request_form_count(void* raw)
{
    xqueryparams* form = xl6_http_request_form_params((XL6_Http_Request*)raw);
    return form != 0 ? (long long)xrtQueryParamsCount(form) : 0;
}

int xl6_xrt_http_request_form_has_value(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* form = xl6_http_request_form_params((XL6_Http_Request*)raw);
    return index >= 0 && form != 0 && xrtQueryParamsAt(form, (size_t)index, &pair) && (pair.Flags & 1u) != 0u;
}

void* xl6_xrt_http_request_form_name(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* form = xl6_http_request_form_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(index >= 0 && form != 0 && xrtQueryParamsAt(form, (size_t)index, &pair) ? pair.Key : xl6_http_view(0));
}

void* xl6_xrt_http_request_form_value_at(void* raw, long long index)
{
    xquerypair pair;
    xqueryparams* form = xl6_http_request_form_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(index >= 0 && form != 0 && xrtQueryParamsAt(form, (size_t)index, &pair) ? pair.Value : xl6_http_view(0));
}

void* xl6_xrt_http_request_form_value(void* raw, const char* name)
{
    xquerypair pair;
    xqueryparams* form = xl6_http_request_form_params((XL6_Http_Request*)raw);
    return xl6_http_copy_view(name != 0 && form != 0 && xrtQueryParamsGet(form, xl6_http_view(name), &pair) ? pair.Value : xl6_http_view(0));
}

int xl6_xrt_http_request_is_multipart_form(void* raw)
{
    return xl6_http_request_media_is((XL6_Http_Request*)raw, "multipart", "form-data");
}

long long xl6_xrt_http_request_multipart_count(void* raw)
{
    xformdata* form = xl6_http_request_form_data((XL6_Http_Request*)raw);
    return form != 0 ? (long long)xrtFormDataCount(form) : 0;
}

static bool xl6_http_request_part_at(XL6_Http_Request* request, long long index, xformdatapart* part)
{
    xformdata* form = xl6_http_request_form_data(request);
    return index >= 0 && form != 0 && xrtFormDataAt(form, (size_t)index, part);
}

int xl6_xrt_http_request_multipart_is_file(void* raw, long long index)
{
    xformdatapart part;
    return xl6_http_request_part_at((XL6_Http_Request*)raw, index, &part) && (part.Flags & 1u) != 0u;
}

void* xl6_xrt_http_request_multipart_name(void* raw, long long index)
{
    xformdatapart part;
    return xl6_http_copy_view(xl6_http_request_part_at((XL6_Http_Request*)raw, index, &part) ? part.Name : xl6_http_view(0));
}

void* xl6_xrt_http_request_multipart_file_name(void* raw, long long index)
{
    xformdatapart part;
    return xl6_http_copy_view(xl6_http_request_part_at((XL6_Http_Request*)raw, index, &part) ? part.Filename : xl6_http_view(0));
}

void* xl6_xrt_http_request_multipart_content_type(void* raw, long long index)
{
    xformdatapart part;
    return xl6_http_copy_view(xl6_http_request_part_at((XL6_Http_Request*)raw, index, &part) ? part.ContentType : xl6_http_view(0));
}

long long xl6_xrt_http_request_multipart_size(void* raw, long long index)
{
    xformdatapart part;
    return xl6_http_request_part_at((XL6_Http_Request*)raw, index, &part) && part.Length <= INT64_MAX ? (long long)part.Length : 0;
}

static xbytesview xl6_http_request_part_bytes(XL6_Http_Request* request, long long index)
{
    xformdatapart part;
    xbytesview bytes = { 0, 0u };
    if ( xl6_http_request_part_at(request, index, &part) ) { (void)xrtHttpBodyView(part.Body, &bytes); }
    return bytes;
}

void* xl6_xrt_http_request_multipart_text(void* raw, long long index)
{
    xbytesview bytes = xl6_http_request_part_bytes((XL6_Http_Request*)raw, index);
    return xl6_xrt_string_dup((const char*)(bytes.Data != 0 ? bytes.Data : (const unsigned char*)""), bytes.Size);
}

void* xl6_xrt_http_request_multipart_bytes(void* raw, long long index)
{
    xbytesview bytes = xl6_http_request_part_bytes((XL6_Http_Request*)raw, index);
    return xl6_xrt_buffer_from_view_bytes(bytes.Data, bytes.Size);
}

void* xl6_xrt_http_request_multipart_value(void* raw, const char* name)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    xformdata* form = xl6_http_request_form_data(request);
    xformdatapart part;
    xbytesview bytes = { 0, 0u };
    if ( name != 0 && form != 0 && xrtFormDataGet(form, xl6_http_view(name), &part) && (part.Flags & 1u) == 0u ) { (void)xrtHttpBodyView(part.Body, &bytes); }
    return xl6_xrt_string_dup((const char*)(bytes.Data != 0 ? bytes.Data : (const unsigned char*)""), bytes.Size);
}

static char* xl6_http_request_address(XL6_Http_Request* request, bool remote)
{
    xnetaddr address;
    char text[96];
    size_t size;
    bool ok = request != 0 && request->Connection != 0 && (remote ? xrtHttpConnRemote(request->Connection, &address) : xrtHttpConnLocal(request->Connection, &address));
    if ( !ok ) { return xl6_xrt_string_dup("", 0u); }
    size = xrtNetAddrText(&address, text, sizeof(text));
    return size < sizeof(text) ? xl6_xrt_string_dup(text, size) : xl6_xrt_string_dup("", 0u);
}

void* xl6_xrt_http_request_local_addr(void* raw) { return xl6_http_request_address((XL6_Http_Request*)raw, false); }
void* xl6_xrt_http_request_remote_addr(void* raw) { return xl6_http_request_address((XL6_Http_Request*)raw, true); }

void* xl6_xrt_http_request_tls_sni(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    xtlsstream* stream = request != 0 && request->Connection != 0 ? xrtHttpConnTls(request->Connection) : 0;
    xtlssession* session = stream != 0 ? xrtTlsStreamSession(stream) : 0;
    xbytesview name = { 0, 0u };
    if ( session != 0 ) { (void)xrtTlsServerName(session, &name); }
    return xl6_xrt_string_dup((const char*)(name.Data != 0 ? name.Data : (const unsigned char*)""), name.Size);
}

static long long xl6_http_request_port(XL6_Http_Request* request, bool remote)
{
    xnetaddr address;
    bool ok = request != 0 && request->Connection != 0 && (remote ? xrtHttpConnRemote(request->Connection, &address) : xrtHttpConnLocal(request->Connection, &address));
    return ok ? (long long)address.Port : 0;
}

long long xl6_xrt_http_request_local_port(void* raw) { return xl6_http_request_port((XL6_Http_Request*)raw, false); }
long long xl6_xrt_http_request_remote_port(void* raw) { return xl6_http_request_port((XL6_Http_Request*)raw, true); }

int xl6_xrt_http_request_connection_open(void* raw)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    int state = request != 0 && request->Connection != 0 ? xrtHttpConnState(request->Connection) : 7;
    return state < 6;
}

int xl6_xrt_http_request_close_connection(void* raw, int abortConnection)
{
    XL6_Http_Request* request = (XL6_Http_Request*)raw;
    return request != 0 && request->Connection != 0 && (abortConnection ? xrtHttpConnAbort(request->Connection) : xrtHttpConnClose(request->Connection));
}
    ) "\n";



/* Reply 负责响应构建；常见文本仍是一条调用，复杂响应可以逐项编辑。 */
