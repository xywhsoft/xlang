static const char xl6WebSocketClientSource[] = XL6_WS_SOURCE(
static void xl6_ws_client_config(const XL6_WsClientOptions* options, xwsclientconfig* config)
{
    xrtWsClientConfigInit(config);
    config->Connection.MessageLimit = options->MessageLimit;
    config->Connection.FrameLimit = options->FrameLimit;
    config->Connection.SendLimit = options->HighWater != 0u && options->HighWater < options->SendLimit ? options->HighWater : options->SendLimit;
    config->Connection.CloseTimeout = options->CloseTimeout;
    config->Connection.AsyncBytesLimit = options->SendLimit;
    config->Connection.Deflater.Level = options->CompressLevel;
    config->Http.Timeout = options->HandshakeTimeout != 0u ? options->HandshakeTimeout : options->ConnectTimeout;
    if ( options->Proxy != 0 ) { config->Http.Proxy.Mode = 2; config->Http.Proxy.Proxy = options->Proxy; }
    config->Protocols = xl6_ws_view(options->Protocols);
    config->EnableDeflate = options->Compression;
}

static bool xl6_ws_client_handshake(XL6_WsClient* client, const xhttpresponse* response)
{
    XL6_WsHandshakeMessage message;
    xvalue* result;
    bool accepted;
    if ( client == 0 || !xl6_ws_function_valid(&client->Callbacks.Handshake) ) { return true; }
    memset(&message, 0, sizeof(message));
    message.Kind = 2;
    message.Response = response;
    result = xl6_function_call_var(client->Callbacks.Handshake, 1, xrtValuePointer(&message));
    accepted = result != 0 && xl6_xrt_value_bool(result);
    xl6_ws_result_release(result);
    return accepted;
}

static void xl6_ws_client_complete(xhttpcall* call, int result, xwsconn* connection, xhttpresponse* response, const xerror* error, void* data)
{
    XL6_WsClient* client = (XL6_WsClient*)data;
    bool accepted;
    xvalue* callbackResult;
    int status = response != 0 ? (int)xrtHttpResponseStatus(response) : 0;
    if ( client == 0 ) { xrtWsConnDestroy(connection); xrtHttpResponseDestroy(response); return; }
    accepted = result == 0 && connection != 0 && xl6_ws_client_handshake(client, response);
    if ( accepted ) {
        client->Connection = connection;
        client->Compression = xl6_ws_deflate_enabled(connection);
        xl6_ws_error_clear(&client->LastError);
        if ( xl6_ws_function_valid(&client->Callbacks.Open) ) {
            callbackResult = xl6_function_call_var(client->Callbacks.Open, 0);
            xl6_ws_result_release(callbackResult);
        }
        (void)xl6_ws_promise_bool(client->OpenPromise, true);
    } else {
        xl6_ws_error_set(&client->LastError, result != 0 ? result : -1, status, 0u, error != 0 ? error : xrtGetError());
        xl6_ws_call_client_error(client);
        if ( connection != 0 ) {
            client->Connection = connection;
            (void)xrtWsConnAbort(connection);
        } else if ( client->EventReference ) {
            client->EventReference = false;
            xl6_ws_client_release(client);
        }
        (void)xl6_ws_promise_bool(client->OpenPromise, false);
    }
    xrtHttpResponseDestroy(response);
    if ( client->Call != 0 ) { xrtHttpCallDestroy(client->Call); client->Call = 0; }
    else if ( call != 0 ) { xrtHttpCallDestroy(call); }
    xrtPromiseDestroy(client->OpenPromise);
    client->OpenPromise = 0;
}

static XL6_WsClient* xl6_ws_client_create(const XL6_WsClientOptions* options, xnetengine* engine)
{
    XL6_WsClient* client;
    if ( options == 0 ) { return 0; }
    client = (XL6_WsClient*)xrtCalloc(1u, sizeof(*client));
    if ( client == 0 ) { return 0; }
    client->References = 1;
    client->Message = xrtBufferCreate();
    if ( !xl6_ws_client_options_copy(&client->Options, options) || client->Message == 0 ) { xl6_ws_client_release(client); return 0; }
    client->Engine = engine;
    if ( client->Engine == 0 ) {
        client->Engine = (xnetengine*)xl6_xrt_net_engine_create(1);
        client->OwnEngine = client->Engine != 0;
        if ( client->Engine == 0 || !xl6_xrt_net_engine_start(client->Engine) ) { xl6_ws_client_release(client); return 0; }
    }
    if ( client->Options.TlsConfig != 0 ) {
        client->TlsLease = xl6_xrt_tls_lease_create(client->Options.TlsConfig);
        if ( client->TlsLease == 0 || client->TlsLease->Invalid ) { xl6_ws_client_release(client); return 0; }
    }
    client->Http = client->TlsLease != 0 ? xrtHttpClientCreateTls(client->Engine, client->TlsLease->Context, client->TlsLease->Verifier) : xrtHttpClientCreate(client->Engine, 0);
    client->CompressMin = client->Options.CompressMin;
    client->Compression = client->Options.Compression;
    if ( client->Http == 0 ) { xl6_ws_client_release(client); return 0; }
    return client;
}

static void xl6_ws_client_release(XL6_WsClient* client)
{
    if ( client == 0 || xrtRefRelease(&client->References) != 0 ) { return; }
    if ( client->Call != 0 ) { (void)xrtHttpCallCancel(client->Call); xrtHttpCallDestroy(client->Call); }
    xrtWsConnDestroy(client->Connection);
    xrtPromiseDestroy(client->OpenPromise);
    xrtFutureDestroy(client->OpenFuture);
    xrtHttpClientDestroy(client->Http);
    xl6_xrt_tls_lease_release(client->TlsLease);
    xrtBufferDestroy(client->Message);
    xl6_ws_client_options_unit(&client->Options);
    xl6_ws_callbacks_unit(&client->Callbacks);
    xl6_ws_error_clear(&client->LastError);
    xl6_ws_close_clear(&client->LastClose);
    if ( client->OwnEngine ) { (void)xl6_xrt_net_engine_destroy(client->Engine); }
    xrtFree(client);
}

static XL6_WsClient* xl6_ws_client_create_url(const char* url, xnetengine* engine)
{
    XL6_WsClientOptions options;
    XL6_WsClient* client;
    if ( !xl6_ws_client_options_init(&options, url) ) { return 0; }
    client = xl6_ws_client_create(&options, engine);
    xl6_ws_client_options_unit(&options);
    return client;
}

void* xl6_xrt_ws_client_create_url(const char* url) { return xl6_ws_client_create_url(url, 0); }
void* xl6_xrt_ws_client_create_options(void* options) { return xl6_ws_client_create((const XL6_WsClientOptions*)options, 0); }
void* xl6_xrt_ws_client_create_url_engine(const char* url, void* engine) { return engine != 0 ? xl6_ws_client_create_url(url, (xnetengine*)engine) : 0; }
void* xl6_xrt_ws_client_create_options_engine(void* options, void* engine) { return engine != 0 ? xl6_ws_client_create((const XL6_WsClientOptions*)options, (xnetengine*)engine) : 0; }
long long xl6_xrt_ws_client_destroy(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; if ( client != 0 ) { if ( client->Call != 0 ) { (void)xrtHttpCallCancel(client->Call); } if ( client->Connection != 0 ) { (void)xrtWsConnAbort(client->Connection); } xl6_ws_client_release(client); } return 0; }

long long xl6_xrt_ws_client_start(void* raw)
{
    XL6_WsClient* client = (XL6_WsClient*)raw;
    xhttprequest* request;
    xwsclientconfig config;
    xwsconnevents events;
    size_t index;
    if ( client == 0 || client->Started || client->Http == 0 || client->Options.Url == 0 ) { return -1; }
    client->Started = true;
    client->OpenPromise = xrtPromiseCreate(&client->OpenFuture, 0);
    if ( client->OpenPromise == 0 || client->OpenFuture == 0 ) { return -1; }
    xl6_ws_client_config(&client->Options, &config);
    request = xrtWsRequestCreate(xl6_ws_view(client->Options.Url));
    if ( request == 0 ) { xl6_ws_error_set(&client->LastError, -1, 0, 0u, xrtGetError()); (void)xl6_ws_promise_bool(client->OpenPromise, false); xrtPromiseDestroy(client->OpenPromise); client->OpenPromise = 0; return -1; }
    for ( index = 0u; index < xrtHttpHeadersCount(client->Options.Headers); ++index ) {
        const xhttpfield* field = xrtHttpHeadersAt(client->Options.Headers, index);
        if ( field == 0 || !xrtHttpRequestAddHeader(request, field->Name, field->Value) ) { xrtHttpRequestDestroy(request); xl6_ws_error_set(&client->LastError, -1, 0, 0u, xrtGetError()); (void)xl6_ws_promise_bool(client->OpenPromise, false); xrtPromiseDestroy(client->OpenPromise); client->OpenPromise = 0; return -1; }
    }
    if ( client->Options.Origin != 0 && client->Options.Origin[0] != 0 && !xrtHttpRequestAddHeader(request, xl6_ws_view("Origin"), xl6_ws_view(client->Options.Origin)) ) { xrtHttpRequestDestroy(request); xl6_ws_error_set(&client->LastError, -1, 0, 0u, xrtGetError()); (void)xl6_ws_promise_bool(client->OpenPromise, false); xrtPromiseDestroy(client->OpenPromise); client->OpenPromise = 0; return -1; }
    xl6_ws_client_events(&events);
    client->EventReference = true;
    (void)xl6_ws_client_retain(client);
    client->Call = xrtWsConnectRequest(client->Http, request, &config, &events, client, xl6_ws_client_complete, client);
    xrtHttpRequestDestroy(request);
    if ( client->Call == 0 ) {
        client->EventReference = false;
        xl6_ws_client_release(client);
        xl6_ws_error_set(&client->LastError, -1, 0, 0u, xrtGetError());
        (void)xl6_ws_promise_bool(client->OpenPromise, false);
        xrtPromiseDestroy(client->OpenPromise);
        client->OpenPromise = 0;
        return -1;
    }
    return 0;
}

xfuture* xl6_xrt_ws_client_start_async(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; if ( client == 0 ) { return 0; } if ( !client->Started && xl6_xrt_ws_client_start(client) != 0 ) { return 0; } return xrtFutureRef(client->OpenFuture); }
void xl6_xrt_ws_client_stop(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; if ( client == 0 ) { return; } if ( client->Call != 0 ) { (void)xrtHttpCallCancel(client->Call); } if ( client->Connection != 0 ) { (void)xrtWsConnAbort(client->Connection); } }
int xl6_xrt_ws_client_open(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && client->Connection != 0 && xrtWsConnState(client->Connection) == 0; }
void* xl6_xrt_ws_client_protocol(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return xl6_ws_copy_view(client != 0 && client->Connection != 0 ? xrtWsConnProtocol(client->Connection) : xl6_ws_view("")); }
int xl6_xrt_ws_client_compressed(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_deflate_enabled(client->Connection); }
long long xl6_xrt_ws_client_pending(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && client->Connection != 0 ? (long long)xrtWsConnPending(client->Connection) : 0; }
long long xl6_xrt_ws_client_send_text(void* raw, const char* data) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_send_text(client->Connection, data, client->CompressMin, client->Compression) : -1; }
long long xl6_xrt_ws_client_send_bytes(void* raw, void* data, size_t size) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_send_binary(client->Connection, data, size, client->CompressMin, client->Compression) : -1; }
long long xl6_xrt_ws_client_ping(void* raw, void* data, size_t size) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_ping(client->Connection, data, size) : -1; }
long long xl6_xrt_ws_client_close(void* raw, long long code, const char* reason) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_close(client->Connection, code, reason) : -1; }
long long xl6_xrt_ws_client_wait_open(void* raw, long long timeout) { XL6_WsClient* client = (XL6_WsClient*)raw; if ( client == 0 || client->OpenFuture == 0 ) { return -1; } if ( !xl6FutureWaitFor(client->OpenFuture, timeout) ) { return -3; } return xl6_xrt_ws_client_open(client) ? 0 : -1; }
long long xl6_xrt_ws_client_wait(void* raw, long long kind, long long timeout) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_wait(client->Connection, kind, timeout) : -1; }
xfuture* xl6_xrt_ws_client_writable_async(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(client->Connection, 1)) : 0; }
xfuture* xl6_xrt_ws_client_drain_async(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(client->Connection, 2)) : 0; }
xfuture* xl6_xrt_ws_client_close_async(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_future_bool(xrtWsConnWaitAsync(client->Connection, 3)) : 0; }
void* xl6_xrt_ws_client_begin_text(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_writer_create(client->Connection, 1u, client->CompressMin, client->Compression) : 0; }
void* xl6_xrt_ws_client_begin_binary(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_writer_create(client->Connection, 2u, client->CompressMin, client->Compression) : 0; }
void* xl6_xrt_ws_client_close_info(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_close_clone(&client->LastClose) : 0; }
void* xl6_xrt_ws_client_error(void* raw) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 ? xl6_ws_error_clone(&client->LastError) : 0; }

int xl6_xrt_ws_client_on_open(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Open, callback, client->Started); }
int xl6_xrt_ws_client_on_text(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Text, callback, client->Started); }
int xl6_xrt_ws_client_on_binary(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Binary, callback, client->Started); }
int xl6_xrt_ws_client_on_close(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Close, callback, client->Started); }
int xl6_xrt_ws_client_on_error(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Error, callback, client->Started); }
int xl6_xrt_ws_client_on_ping(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Ping, callback, client->Started); }
int xl6_xrt_ws_client_on_pong(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Pong, callback, client->Started); }
int xl6_xrt_ws_client_on_backpressure(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Backpressure, callback, client->Started); }
int xl6_xrt_ws_client_on_writable(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Writable, callback, client->Started); }
int xl6_xrt_ws_client_on_drain(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Drain, callback, client->Started); }
int xl6_xrt_ws_client_on_handshake(void* raw, xvalue* callback) { XL6_WsClient* client = (XL6_WsClient*)raw; return client != 0 && xl6_ws_function_set_value(&client->Callbacks.Handshake, callback, client->Started); }
) "\n";



/* 服务端只负责 HTTP 路由与 Upgrade 编排，协议检查和 Connection 接管均由 XRT 完成。 */
